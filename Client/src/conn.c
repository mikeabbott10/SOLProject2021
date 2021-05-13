#include<conn.h>

/**
 * Write the string str on the file descriptor fd
 * @param fd: the file descriptor
 * @param str: the string to write
 * @return -1 if error occurred (errno is setted up), 0 if write returns 0, size if it ends up successfully
 */
int sendStringTo(int fd, char* str){
    msg_t msg;
    msg.len = strlen(str);
    ec( msg.str = strndup(str, strlen(str)), NULL, return -1);
    char *msgLenAsString;
    if( (msgLenAsString = intToStr(msg.len)) == NULL){
        free(msg.str);
        return -1;
    }
    char *string;
    if( (string = calloc(strlen(msgLenAsString) + 1 + msg.len + 1, sizeof(char))) == NULL){
        free(msgLenAsString);
        free(msg.str);
        return -1;
    }
    strncat(string, msgLenAsString, strlen(msgLenAsString));
    strncat(string, msg.str, msg.len);
    printf("SCRIVO:\n%s", string); // debug
    fflush(stdout);
    errno = 0;
    int writenRet = writen(fd, string, strlen(string));
    if(writenRet == -1){
        perror("writen(fd, string, strlen(string))");
        free(string);
        free(msgLenAsString);
        free(msg.str);
        return -1;
    }
    free(string);
    free(msgLenAsString);
    free(msg.str);
    return writenRet;
}

/*----------- I/O utils ----------------------------------------*/
/**
 * Read "size" bytes from a descriptor avoiding partial reads
 * @param fd: the file descriptor
 * @param buf: the buffer we are going to fill
 * @param size: the number of bytes we want to read
 * @return -1 if error occurred (errno is setted up), 0 if EOF is read, size if it ends up successfully
 */
int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
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
 * Get the server response
 * @param socketFD: the socket fd
 * @return 
 *      -2 if parsing error occurred
 *      -1 if fatal error occurred (errno is setted up), 
 *      0 if EOF is read,
 *      1 if it ends up successfully
 */
int getServerMessage(int socketFD, char **msg){
    char *msgLenBuf, *msgBuf;
    int msgLen, bytesRead;
    
    ec( msgLenBuf = calloc(9 + 1, sizeof(char)), NULL, return -1 );
    if((bytesRead = readn(socketFD, msgLenBuf, 9)) <= 0){
        free(msgLenBuf);
        return bytesRead; /* return -1 or 0 */
    }
    printf("Len:\n%s\n", msgLenBuf);
    if( isInteger(msgLenBuf, &msgLen) != 0){
        free(msgLenBuf);
        return -2;
    }

    if( (msgBuf = calloc(msgLen + 1, sizeof(char))) == NULL){
        free(msgLenBuf);
        return -1;
    }
    /*note that we trust our server (just a school project)*/
    if((bytesRead = readn(socketFD, msgBuf, msgLen)) <= 0){
        free(msgLenBuf);
        free(msgBuf);
        return bytesRead; /* return -1 or 0 */
    }
    printf("Msg:\n%s\n", msgBuf);

    free(msgLenBuf);
    *msg = msgBuf;
    return 1;
}