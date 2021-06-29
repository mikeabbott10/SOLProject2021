#include<conn.h>

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
    
    ec( msgLenBuf = calloc(MSG_LEN_LENGTH + 1, sizeof(char)), NULL, return -1 );
    if((bytesRead = readn(socketFD, msgLenBuf, MSG_LEN_LENGTH)) <= 0){
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
    /*note that we trust our server*/
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
    if( (msgLenAsString = intToStr(msg.len, MSG_LEN_LENGTH)) == NULL)
        return -1;
    if( (msg.content = calloc(strlen(msgLenAsString) + msg.len + 1, sizeof(char))) == NULL ){
        free(msgLenAsString);
        return -1;
    }

    msg.content = memmove(msg.content, msgLenAsString, strlen(msgLenAsString));
    memmove(msg.content+strlen(msgLenAsString), content, msg.len);

    /*update msg.len*/
    msg.len = strlen(msgLenAsString) + msg.len;
    /*printf("Invio->");
    fwrite(msg.content, 1, msg.len, stdout); // debug
    puts("");
    fflush(stdout);*/
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

/* we make a NULL msg_t value on an existinf msg_t */
msg_t getNullMessage(msg_t *msg){
    msg->content = NULL;
    msg->len = 0;
    return *msg;
}

/**
 * Build a message in order to send it to the other side.
 * @param req: the action to perform
 * @param flags: the flags of the request
 * @param filePath: the path of the file we want to perform an action on
 * @param content: the content of the file
 * @param contentLength: the content length in bytes
 * @return the msg_t
 */ 
msg_t buildMessage(char req, char flags, const char* filePath, char* content, int contentLength){
    /*
      Format: "AAAAFFFFLLLLLLLLLfilePathLLLLLLLLLcontent"
      Meaning:  AAAA -> 4 bytes representing the action to perform
                FFFF -> 4 bytes representing the flags of the request
                LLLLLLLLL -> 9 bytes representing the length of the file path in bytes
                filePath -> the filePath
                LLLLLLLLL -> 9 bytes representing the length of the content in bytes
                content -> the file content
    */
    msg_t msg;
    /*req*/
    char *reqAsString;
    if( (reqAsString = intToStr(req, 4)) == NULL)
        return getNullMessage(&msg);
    /*flags*/
    char *flagsAsString;
    if( (flagsAsString = intToStr(flags, 4)) == NULL){
        free(reqAsString);
        return getNullMessage(&msg);
    }
    /*filePath length*/
    char *filePathLengthAsString;
    int pathLength = strlen(filePath);
    if( (filePathLengthAsString = intToStr(pathLength, 9)) == NULL){
        free(flagsAsString);
        free(reqAsString);
        return getNullMessage(&msg);
    }
    /*content length*/
    char *contentLengthAsString;
    if( (contentLengthAsString = intToStr(contentLength, 9)) == NULL){
        free(filePathLengthAsString);
        free(flagsAsString);
        free(reqAsString);
        return getNullMessage(&msg);
    }

    if( (msg.content = calloc(4 + 4 + 9 + pathLength + 9 + contentLength + 1, sizeof(char))) == NULL ){
        free(contentLengthAsString);
        free(filePathLengthAsString);
        free(flagsAsString);
        free(reqAsString);
        return getNullMessage(&msg);
    }

    msg.content = memmove(msg.content, reqAsString, 4);
    msg.len = 4;
    memmove(msg.content + msg.len, flagsAsString, 4);
    msg.len += 4;
    memmove(msg.content + msg.len, filePathLengthAsString, 9);
    msg.len += 9;
    memmove(msg.content + msg.len, filePath, pathLength);
    msg.len += pathLength;
    memmove(msg.content + msg.len, contentLengthAsString, 9);
    msg.len += 9;
    if(contentLength != 0)
        memmove(msg.content + msg.len, content, contentLength);
    msg.len += contentLength;

    free(contentLengthAsString);
    free(filePathLengthAsString);
    free(flagsAsString);
    free(reqAsString);
    return msg;
}