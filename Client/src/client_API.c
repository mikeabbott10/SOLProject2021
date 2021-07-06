#include<general_utility.h>
#include<client_API.h>
#include<conn.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* ind AF_UNIX */
#define UNIX_PATH_MAX 108 /* man 7 unix */

/**
 * Open an AF_UNIX connection to the socket file sockname.
 * @param sockname: the socket path
 * @param msec: waiting time after a failed connection request
 * @param abstime: won't try to connect again after abstime
 * @return: 0 if a connection is established, -1 if something wrong (errno is setted up)
 */
int openConnection(const char* sockname, int msec, const struct timespec abstime){
    ec( sockfd = socket(AF_UNIX, SOCK_STREAM, 0), -1, return EXIT_FAILURE );

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, sockname, strlen(sockname)+1);

    ec( sockPath = calloc(strlen(sockname)+1, sizeof(char)), NULL, return EXIT_FAILURE );
    strncpy(sockPath, sockname, strlen(sockname)+1);

    /*get how many ms we can try on*/
    uint64_t stop_abs_ms = abstime.tv_sec * 1000 + abstime.tv_nsec / 1e6;
    uint64_t abs_ms = get_now_time();
    int64_t remaining_ms = stop_abs_ms - abs_ms;
     
    errno = 0;
    int res;
    while ( (res=connect(sockfd,(struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1 && remaining_ms > 0) { /*keep trying*/
        msleep(msec);
        abs_ms = get_now_time();
        remaining_ms = stop_abs_ms - abs_ms;
        /*printf("remains: %ld\t", remaining_ms);*/
    }
    return res;
}

/**
 * Close the connection to the socket sockname if it's already opened.
 * @return: 0 if the connection is closed, -1 if something wrong (errno is setted up)
 */ 
int closeConnection(const char* sockname){
    if(strncmp(sockname, sockPath, strlen(sockPath))==0){
        free(sockPath);
        return close(sockfd);
    }else{
        errno = EBADF;
        return -1;
    }
}

/**
 * Send an open-file request or a create-file request to the server. It depends on flags.
 * @param pathname: the path of the file on the server
 * @param flags: specify the open mode
 * @return: 0 if file is opened, -1 if we fail (errno is setted up)
 */
int openFile(const char* pathname, int flags){
    msg_t msg = buildMessage(OPEN, flags, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    int retVal = -1;
    char *response = NULL;
    if(getServerMessage(sockfd, &response)==1){
        if(strncmp(response, FINE_REQ_RESPONSE, 3)==0)
            retVal = 0;
        else if(strncmp(response, WRONG_FILEPATH_RESPONSE, 3)==0)
            errno = ENOENT;
        else if(strncmp(response, NOT_PERMITTED_ACTION_RESPONSE, 3)==0)
            errno = EPERM;
    }
    if(response!=NULL) free(response);
    free(msg.content);
    return retVal;
}

/**
 * Send a close-file request to the server. Any action on the 
 * pathname file after the closeFile fails.
 * @param pathname: the path of the file on the server
 * @return: 0 if file is closed, -1 if we fail (errno is setted up)
 */
int closeFile(const char* pathname){
    msg_t msg = buildMessage(CLOSE, NO_FLAGS, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    int retVal = -1;
    char *response = NULL;
    if(getServerMessage(sockfd, &response)==1){
        if(strncmp(response, FINE_REQ_RESPONSE, 3)==0)
            retVal = 0;
        else if(strncmp(response, WRONG_FILEPATH_RESPONSE, 3)==0)
            errno = ENOENT;
        else if(strncmp(response, NOT_PERMITTED_ACTION_RESPONSE, 3)==0)
            errno = EPERM;
    }
    if(response!=NULL) free(response);
    free(msg.content);
    return retVal;
}

/**
 * Send a lock-file request to the server.
 * @param pathname: the path of the file on the server
 * @return: 0 if file is locked or the request is pending, -1 if we fail (errno is setted up)
 */
int lockFile(const char* pathname){
    msg_t msg = buildMessage(LOCK, NO_FLAGS, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    int retVal = -1;
    char *response = NULL;
    if(getServerMessage(sockfd, &response)==1){
        if(strncmp(response, FINE_REQ_RESPONSE, 3)==0)
            retVal = 0;
        else if(strncmp(response, PENDING_REQ_RESPONSE, 3)==0)
            retVal = 0;
        else if(strncmp(response, WRONG_FILEPATH_RESPONSE, 3)==0)
            errno = ENOENT;
        else if(strncmp(response, NOT_PERMITTED_ACTION_RESPONSE, 3)==0)
            errno = EPERM;
    }
    if(response!=NULL) free(response);
    free(msg.content);
    return retVal;
}

/**
 * Send an unlock-file request to the server.
 * @param pathname: the path of the file on the server
 * @return: 0 if file is unlocked, -1 if we fail (errno is setted up)
 */
int unlockFile(const char* pathname){
    msg_t msg = buildMessage(UNLOCK, NO_FLAGS, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    int retVal = -1;
    char *response = NULL;
    if(getServerMessage(sockfd, &response)==1){
        if(strncmp(response, FINE_REQ_RESPONSE, 3)==0)
            retVal = 0;
        else if(strncmp(response, WRONG_FILEPATH_RESPONSE, 3)==0)
            errno = ENOENT;
        else if(strncmp(response, NOT_PERMITTED_ACTION_RESPONSE, 3)==0)
            errno = EPERM;
    }
    if(response!=NULL) free(response);
    free(msg.content);
    return retVal;
}

/**
 * Send a remove-file request to the server.
 * @param pathname: the path of the file on the server
 * @return: 0 if file is removed, -1 if we fail (errno is setted up)
 */
int removeFile(const char* pathname){
    msg_t msg = buildMessage(REMOVE, NO_FLAGS, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    int retVal = -1;
    char *response = NULL;
    if(getServerMessage(sockfd, &response)==1){
        if(strncmp(response, FINE_REQ_RESPONSE, 3)==0)
            retVal = 0;
        else if(strncmp(response, WRONG_FILEPATH_RESPONSE, 3)==0)
            errno = ENOENT;
        else if(strncmp(response, NOT_PERMITTED_ACTION_RESPONSE, 3)==0)
            errno = EPERM;
    }
    if(response!=NULL) free(response);
    free(msg.content);
    return retVal;
}

/*
int readFile(const char* pathname, void** buf, size_t* size){

}

int readNFiles(int N, const char* dirname){

}

int writeFile(const char* pathname, const char* dirname){

}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){

}*/