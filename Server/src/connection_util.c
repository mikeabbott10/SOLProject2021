#define _POSIX_C_SOURCE 200809L
#include<connection_util.h>
#include<general_utility.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include <string.h>
#include <unistd.h>
#include<sys/time.h>

/*------------ Thread utils -----------------------------------*/
/**
 * Create "n" worker threads performing function F and save to worker_threads
 * @param n: the number of threads to spawn
 * @param F: the thread function
 * @return 1 is something wrong, 0 otherwise
 */
char spawnWorkers(int n, workerFun F){
    ec( worker_threads = malloc(n * sizeof(pthread_t)), NULL, return EXIT_FAILURE);
    int i = 0;
    for(;i<n; ++i){
        ec_n( pthread_create(&(worker_threads[i]), NULL, F, NULL), 0, exit(EXIT_FAILURE) );
    }
    return EXIT_SUCCESS;
}

/**
 * Perform a clean exit from the master thread
 * @param listen_fd: the file descriptor to close
 */
void master_clean_exit(int listen_fd, int* int_arr){
    globalQuit = 1;
    if(master_progress>=1) close(listen_fd);
    if(master_progress>=2) free(int_arr);
    shared_buffer_artificial_signal(client_fd_buffer, client_fd_t, clientBuffer);
    pthread_exit(NULL);
    //exit(EXIT_FAILURE);
}

/**
 * Get the highest index from active descriptors
 * @return the highest index
 */
int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
    	if (FD_ISSET(i, &set)) return i;
    return -1; /*never reached*/
}

/**
 * Get fd from pipe and add it to a fd set
 * @param set: the set we add the fd to
 * @return 1 if something wrong, 0 otherwise
 */
/*char clientFDFromPipeToSet(fd_set *set){
    safe_mutex_lock(&(pipe_mutex));
    int fd_from_pipe;
    if(read(pipefd[0], &fd_from_pipe, sizeof(fd_from_pipe)) <= 0){
        //unlock everything but exit with error
        pthread_cond_signal(&(pipe_not_read_yet));
        safe_mutex_unlock(&(pipe_mutex));
        return EXIT_FAILURE;
    } 
    FD_SET(fd_from_pipe, set);  // fd added to the set
    currentPipeMsgCount --;
    pthread_cond_signal(&(pipe_not_read_yet));
    safe_mutex_unlock(&(pipe_mutex));
    return EXIT_SUCCESS;
}*/

/*----------- I/O utils ----------------------------------------*/
/**
 * Read "size" bytes from a descriptor avoiding partial reads
 * @param fd: the file descriptor
 * @param buf: the buffer we are going to fill
 * @param size: the number of bytes we want to read
 * @return 
 *      -1 if error occurred (errno is setted up), 
 *      0 if EOF is read (other side closed connection before we could receiving size bytes),
 *      size if it ends up successfully
 */
int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    errno = 0;
    while(left>0) {
        if ((r=read((int)fd ,bufptr,left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;   // EOF
        left -= r;
        bufptr += r;
    }
    return size;
}

/**
 * Write "size" bytes to a descriptor avoiding partial writes
 * @param fd: the file descriptor
 * @param buf: the source buffer
 * @param size: the number of bytes we want to write
 * @return -1 if error occurred (errno is setted up), 0 if write returns 0, size if it ends up successfully
 */
int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
        if ((r=write((int)fd ,bufptr, left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;  
            left -= r;
        bufptr += r;
    }
    return 1;
}

/**
 * Write content on the file descriptor fd
 * @param fd: the file descriptor
 * @param content: content to write
 * @param length: the content length in bytes
 * @return -1 if error occurred (errno is setted up), 0 if write returns 0, 1 if it ends up successfully
 */
int sendTo(int fd, char* content, int length){
    msg_t msg;
    msg.len = length;
    char *msgLenAsString;
    if( (msgLenAsString = intToStr9(msg.len)) == NULL)
        return -1;
    if( (msg.content = calloc(strlen(msgLenAsString) + msg.len + 1, sizeof(char))) == NULL ){
        free(msgLenAsString);
        return -1;
    }
    if( (msg.content = memmove(msg.content, msgLenAsString, strlen(msgLenAsString))) == NULL ){
        free(msgLenAsString);
        free(msg.content);
        errno = EFAULT;
        return -1;
    }
    if( memmove(msg.content+strlen(msgLenAsString), content, msg.len) == NULL ){
        free(msgLenAsString);
        free(msg.content);
        errno = EFAULT;
        return -1;
    }

    /*update msg.len*/
    msg.len = strlen(msgLenAsString) + msg.len;
    printf("Invio->");
    fwrite(msg.content, 1, msg.len, stdout); // debug
    puts("");
    fflush(stdout);
    errno = 0;
    int writenRet = writen(fd, msg.content, msg.len);
    if(writenRet == -1){
        free(msgLenAsString);
        free(msg.content);
        return -1;
    }
    free(msgLenAsString);
    free(msg.content);
    return 1;
}

/**
 * Handle the client request and store it in request
 * @param clientFD: the client socket
 * @param request: the request to override
 * @return 
 *      -2 if parsing error occurred
 *      -1 if fatal error occurred (errno is setted up), 
 *      0 if EOF is read, 
 *      1 if it ends up successfully
 */
int getClientRequest(int clientFD, request_t* request){
    char *msgLenBuf, *msgBuf;
    int msgLen, bytesRead;

    ec( msgLenBuf = calloc(MSG_LEN_LENGTH + 1, sizeof(char)), NULL, return -1 );
    if((bytesRead = readn(clientFD, msgLenBuf, MSG_LEN_LENGTH)) <= 0){
        free(msgLenBuf);
        return bytesRead; /* return -1 or 0 */
    }
    printf("Len:%s\n", msgLenBuf);
    if( isInteger(msgLenBuf, &msgLen) != 0){
        free(msgLenBuf);
        return -2;
    }

    if( (msgBuf = calloc(msgLen + 1, sizeof(char))) == NULL){
        free(msgLenBuf);
        return -1;
    }
    /*note that we trust our server (just a school project)*/
    if((bytesRead = readn(clientFD, msgBuf, msgLen)) <= 0){
        free(msgLenBuf);
        free(msgBuf);
        return bytesRead; /* return -1 or 0 */
    }
    printf("Msg:%s\n", msgBuf);

    //TODO
    //parseMessage(msgBuf, request);
    //msg_t msg = getResponse();

    free(msgLenBuf);
    free(msgBuf);
    return 1;
}