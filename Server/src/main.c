#include<server_config.h>
#include<general_utility.h>
#include<connection_util.h>
#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include <unistd.h>
#include<errno.h>
#include <sys/types.h>
#include <sys/select.h>
#include<sys/socket.h>
#include <sys/un.h> /* ind AF_UNIX */
#define UNIX_PATH_MAX 108 /* man 7 unix */

/* get up the progress levels (we use them for clean quits) */
#define progress_level_up progress_level++
#define master_progress_level_up master_progress++

void* signal_handler_fun(void*);
void* workers_fun(void*);
void* master_fun(void*);
void quit();

int main(int args, char** argv){
    atexit(quit);

    /*get information from config file*/
    ec(parseConfigFile("./src/config.txt"), EXIT_FAILURE, exit(EXIT_FAILURE) );
    progress_level_up;
    
    /*create the (bounded) client_fd buffer, it will be shared between master and worker threads*/
    ec(clientBuffer = shared_buffer_create(client_fd_buffer, client_fd_t, config_struct.max_connections_buffer_size), NULL, 
        exit(EXIT_FAILURE) 
    );
    progress_level_up;

    /*initialize the signal mask*/
    procCurrentMask = initSigMask(); /*if it fails exit(EXIT_FAILURE) is called*/
    
    /*init pipe to get messages from the workers*/
    ec( pipe(pipefd), -1, exit(EXIT_FAILURE) );
    progress_level_up;

    /*init pipe to get messages from signal handler*/
    ec( pipe(signalPipefd), -1, exit(EXIT_FAILURE) );
    progress_level_up;

    /*start signal handler thread (sig_handler_tid is the id)*/
    ec_n( pthread_create(&sig_handler_tid, NULL, signal_handler_fun, NULL), 0, exit(EXIT_FAILURE) );
    
    /*start the master thread*/
    ec_n( pthread_create(&master_tid, NULL, master_fun, NULL), 0, exit(EXIT_FAILURE) );

    /*start the worker threads*/
    ec( spawnWorkers(config_struct.worker_threads, &workers_fun), EXIT_FAILURE, exit(EXIT_FAILURE) );
    progress_level_up;


    int i = 0;
    for(;i<config_struct.worker_threads ; ++i)
        pthread_join(worker_threads[i], NULL);
    /*if we are here after worker threads died prematurely, send SIGINT to the signal handler thread*/
    if(!globalQuit)
        pthread_kill(sig_handler_tid, SIGINT);
    pthread_join(sig_handler_tid, NULL);
    pthread_join(master_tid, NULL);

    return EXIT_SUCCESS;
}

/**
 * Keep accessing the client buffer and perform client actions. This is a thread function
 * @param p: the args passed to the thread
 */
void* workers_fun(void* p){
    int connStatus;
    request_t request;
    client_fd_t clientFD;
    while(!globalQuit){
        /* get a client fd, if -1 is returned then we are quitting*/
        if( (clientFD = shared_buffer_get(client_fd_buffer, client_fd_t, clientBuffer, (client_fd_t)-1)) == -1 )
            break;
        
        connStatus = getClientRequest(clientFD, &request);
        printf("getClientRequest returns: %d\n", connStatus);
        fflush(stdout);
        if(connStatus == 0 || connStatus == -2){
            close(clientFD);
            continue;
        }else if(connStatus == -1) 
            /*client closed connection while server was writing to it OR one allocation failed*/
            continue;
        
        /*we got a request*/
        // TODO: eseguo l'azione richiesta dal client
        if( sendTo(clientFD, "NPA", 3)!=1)
            /*client closed connection while server was writing to it*/
            continue;
        

        /*reset request allocated memory*/
        memset(&request, 0, sizeof(request));
        /*get this client fd back to the master, if -1 is returned then we cry a hero here because of an internal error*/
        ec( write(pipefd[1], &clientFD, sizeof(clientFD)), -1, break );
    }
    return NULL;
}

/**
 * Keep listening to incoming requests and push them to the buffer of connections. This is a thread function
 * @param p: the args passed to the thread
 */
void * master_fun(void* p){
    /*init socket*/
    int listenfd;
    ec( listenfd = socket(AF_UNIX, SOCK_STREAM, 0), -1, return NULL );
    master_progress_level_up;
    
    /*bind and listen on listenfd*/
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, config_struct.socket_path, strlen(config_struct.socket_path)+1);
    errno = 0;
    ec( bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), -1, master_clean_exit(listenfd, NULL));
    errno = 0;
    ec( listen(listenfd, SOMAXCONN), -1, master_clean_exit(listenfd, NULL));
    
    /*init set and temporary set*/
    fd_set set, tmpset;
    FD_ZERO(&set);
    FD_ZERO(&tmpset);

    /* add listenfd and pipefd to the master set */
    FD_SET(listenfd, &set);
    FD_SET(pipefd[0], &set);
    FD_SET(signalPipefd[0], &set);

    /* keep track of the highest index file descriptor*/
    int fdmax = max(3, listenfd, pipefd[0], signalPipefd[0]);
    
    int i;
    int *fds_from_pipe = calloc(100, sizeof(int));
    master_progress_level_up;
    
    while(quit_level != HARD_QUIT) {
        if(quit_level == SOFT_QUIT && currentClientConnections == 0)
            break;
        
    	tmpset = set;
        ec( select(fdmax+1, &tmpset, NULL, NULL, NULL), -1, master_clean_exit(listenfd, fds_from_pipe) );
    	/* what fd did we get a request from?*/
    	for(int fd=0; fd <= fdmax; fd++) {
    	    if (FD_ISSET(fd, &tmpset)) {
        		long connfd;
        		if (fd == listenfd && quit_level == 0) { /* new connection */
                    ec( connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL), -1, master_clean_exit(listenfd, fds_from_pipe) );
                    currentClientConnections++;
                    FD_SET(connfd, &set);  /* fd added to the master set*/
                    if(connfd > fdmax) fdmax = connfd;  /* get the highest fd index*/
                    continue;
        		}else if(fd == pipefd[0] && quit_level != HARD_QUIT){
                    read(pipefd[0], fds_from_pipe, 100*sizeof(int));
                    for(i = 0;fds_from_pipe[i]!=0;++i){
                        FD_SET(fds_from_pipe[i], &set);  /* fd added to the master set*/
                        if(fds_from_pipe[i]>fdmax) fdmax = fds_from_pipe[i];
                        fds_from_pipe[i] = 0;
                    }
                    continue;
                }else if(fd == signalPipefd[0]){
                    puts("\t master got a termination signal");
                    if(quit_level == HARD_QUIT){
                        break;
                    }else continue;
                }

        		connfd = fd;  /* already connected client*/
                /* remove fd from set*/
                FD_CLR(connfd, &set); 
		        if (connfd == fdmax) fdmax = updatemax(set, fdmax);
                /*push connfd to the client fd buffer*/
                shared_buffer_put(client_fd_buffer, client_fd_t, clientBuffer, connfd);
    	    }
    	}
    }
    /*signal to every waiting thread*/
    globalQuit = 1;
    //pthread_cond_signal(&(pipe_not_read_yet));
    shared_buffer_artificial_signal(client_fd_buffer, client_fd_t, clientBuffer);
    free(fds_from_pipe);
    close(listenfd);
    return NULL;
}

/**
 * Keep listening to incoming signals. This is a thread function
 * @param p: the args passed to the thread
 */
void * signal_handler_fun(void* p){
    int sig;
    while(1){
        sigwait(&procCurrentMask, &sig); // non funziona se il segnale è destinato direttamente a un thread
        if(sig == SIGINT || sig == SIGQUIT){
            /*kill the process asap. No more accepts. Close the active connections now. (Print the stats)*/
            quit_level = HARD_QUIT;
            ec( write(signalPipefd[1], &quit_level, sizeof(quit_level)), -1, exit(EXIT_FAILURE) );
            break;
        }else if(sig == SIGHUP){
            /*kill the process. No more accepts. Wait for clients to close connections. (Print the stats)*/
            quit_level = SOFT_QUIT;
            ec( write(signalPipefd[1], &quit_level, sizeof(quit_level)), -1, exit(EXIT_FAILURE) );
            break;
        }
    }
    return NULL;
}

/**
 * Cleaning up routine. Frees allocated memory, restore process original mask.
 */
void quit(){
    if(progress_level>=1) free(config_struct.socket_path);
    if(progress_level>=2) shared_buffer_free(client_fd_buffer, client_fd_t, clientBuffer);
    if(progress_level>=3) { close(pipefd[0]); close(pipefd[1]); }
    if(progress_level>=4) { close(signalPipefd[0]); close(signalPipefd[1]); }
    if(progress_level>=5) { free(worker_threads); }

    //restoreOldMask();
}