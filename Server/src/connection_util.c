#define _POSIX_C_SOURCE 200809L
#include<connection_util.h>
#include<general_utility.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include <string.h>
#include <unistd.h>
#include<sys/time.h>
#include<filesystem_util.h>

/*------------ Thread utils ------------------------------------------------------------------------*/
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

/*----------- I/O utils ----------------------------------------------------------------------------*/
/**
 * Handle the client request and store it in request
 * @param clientFD: the client socket
 * @param request: the request to override
 * @return 
 *      -3 if no space for allocation,
 *      -2 if parsing error occurred,
 *      -1 if fatal error occurred (errno is setted up), 
 *      0 if EOF is read, 
 *      1 if it ends up successfully
 */
int getClientRequest(int clientFD, request_t* request){
    char *msgLenBuf, *msgBuf;
    int msgLen, bytesRead;

    ec( msgLenBuf = calloc(MSG_LEN_LENGTH + 1, sizeof(char)), NULL, return -3 );
    if((bytesRead = readn(clientFD, msgLenBuf, MSG_LEN_LENGTH)) <= 0){
        free(msgLenBuf);
        return bytesRead; /* return -1 or 0 */
    }
    /*printf("Len:%s\n", msgLenBuf);*/
    if( isInteger(msgLenBuf, &msgLen) != 0){
        free(msgLenBuf);
        return -2;
    }

    if( (msgBuf = calloc(msgLen + 1, sizeof(char))) == NULL){
        free(msgLenBuf);
        return -3;
    }
    /*note that we trust our server*/
    if((bytesRead = readn(clientFD, msgBuf, msgLen)) <= 0){
        free(msgLenBuf);
        free(msgBuf);
        return bytesRead; /* return -1 or 0 */
    }
    /*printf("Msg:%s\n", msgBuf);*/

    int parseRet = parseMessage(msgBuf, bytesRead, request);
    free(msgLenBuf);
    free(msgBuf);

    request->client_fd = clientFD;
    return parseRet;
}

/**
 * Parse the client request message
 * @param message: the string to parse
 * @param len: the message length
 * @param request: the pointer to the request "object" to set up
 * @return:
 *      -3 if no space for allocation (errno is setted up), 
 *      -2 if parsing error occurred,
 *      1 if it ends up successfully
*/
int parseMessage(char* message, int len, request_t* request){
    /*
      Format: "AAAAFFFFLLLLLLLLLfilePathLLLLLLLLLcontent"
      Meaning:  AAAA -> 4 bytes representing the action to perform
                FFFF -> 4 bytes representing the flags of the request
                LLLLLLLLL -> 9 bytes representing the length of the file path in bytes
                filePath -> the filePath
                LLLLLLLLL -> 9 bytes representing the length of the content in bytes
                content -> the file content
    */
    /*action*/
    char *temp;
    if( (temp = strndup(message, 4)) == NULL) return -3;
    int action;
    if( isInteger(temp, &action) != 0 || action < 0x03 || action > 0x0b){
        free(temp);
        return -2;
    }
    request->action = (char) action;
    free(temp);
    /*flags*/
    message+=4; /*first F*/
    if( (temp = strndup(message, 4)) == NULL) return -3;
    int flags;
    if( isInteger(temp, &flags) != 0 || flags < 0x00 || flags > 0x03){
        free(temp);
        return -2;
    }
    request->action_flags = (char) flags;
    free(temp);

    /*path length*/
    message+=4; /*first L*/
    if( (temp = strndup(message, 9)) == NULL) return -3;
    int path_len;
    if( isInteger(temp, &path_len) != 0 || path_len < 0){
        free(temp);
        return -2;
    }
    free(temp);
    /*path*/
    message+=9; /*first filePath char*/
    if(path_len != 0){
        if( (request->action_related_file_path = strndup(message, path_len)) == NULL) 
            return -3;
    }else
        request->action_related_file_path = NULL;


    /*content length*/
    message+=path_len; /*first L*/
    if( (temp = strndup(message, 9)) == NULL) return -3;
    int content_len;
    if( isInteger(temp, &content_len) != 0 || content_len < 0){
        free(temp);
        return -2;
    }
    free(temp);
    /*content*/
    message+=9; /*first content char*/
    if(content_len != 0){
        if( (request->content = strndup(message, content_len)) == NULL) 
            return -3;
    }else
        request->content = NULL;

    return 1;
}

/* we make a NULL msg_t value on an existinf msg_t */
void getNullMessage(msg_t *msg){
    msg->content = NULL;
    msg->len = 0;
}

/**
 * Perform an action and build a response
 * @param req: the request we get the action from
 * @param msgPtr: the message representing the response for the client side
 * @return: 
 *      -1 if fatal error occurred (i.e. memory error) (errno is setted up)
 *      0 if logic error occurred (i.e. wrong file path...)
 *      1 if the action ends up successfully
 */ 
int performActionAndGetResponse(request_t req, msg_t* msgPtr){
    switch(req.action){
        case OPEN:
            return openFile(msgPtr, req.client_fd, req.action_flags, req.action_related_file_path);

        case CLOSE:
            return closeFile(msgPtr, req.client_fd, req.action_related_file_path);

        case READ:
            return readFile(msgPtr, req.client_fd, req.action_related_file_path);

        case READ_N:{
            // req.content here is the number of files the client wants to read
            int n;
            if(isInteger(req.content, &n) != 0){
                ec( msgPtr->content = calloc( 12 ,sizeof(char)), NULL, return -1 );
                strcpy(msgPtr->content, "Not valid N");
                msgPtr->len = 11;
                break;
            }
            else
                return readNFiles(msgPtr, req.client_fd, n);
        }

        case WRITE:
            return writeFile(msgPtr, req.client_fd, req.action_related_file_path, req.content);

        case APPEND:
            return appendToFile(msgPtr, req.client_fd, req.action_related_file_path, req.content);

        case LOCK:
            return lockFile(msgPtr, req.client_fd, req.action_related_file_path);

        case UNLOCK:
            return unlockFile(msgPtr, req.client_fd, req.action_related_file_path);

        case REMOVE:
            return removeFile(msgPtr, req.client_fd, req.action_related_file_path);
    }
    return 0;
}