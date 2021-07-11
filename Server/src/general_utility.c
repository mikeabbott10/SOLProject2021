#include<general_utility.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<limits.h>
#include <stdarg.h>
#include <math.h>
#include<string.h>
#include <limits.h>

/**
 * Checks if the evaluation of s is an integer and puts the value in n.
 * @param s: char* to evaluate
 * @param n: int* the evaluated value of s goes to
 * @return 0 if s is an integer, 2 overflow or underflow detected, 1 if s is not a number
 */
char isInteger(const char* s, int* n){
    char *e = NULL;
    errno = 0;
    long val = strtol(s, &e, 10);

    if (errno == ERANGE || val > INT_MAX || val < INT_MIN){
        // overflow/underflow
        return 2;
    }

    if (errno==0 && e != NULL && e != s){
        *n = (int) val;
        // è un numero valido
        return 0;
    }

    // non è un numero
    return 1;
}

/**
 * check for overflow after multiplication
 * @param a: first factor
 * @param b: 2nd factor
 * @return
 *      0 if no overflow detected
 *      -1 otherwise
 */
int checkMultiplicationOverflow(int a, int b){
    if(b==0) return 0;
    if (a > INT_MAX / b) return -1; // a * b would overflow
    if ((a < INT_MIN / b)) return -1;// `a * b` would underflow
    return 0;
}

/**
 * Find maximum between two or more integer variables
 * @param args Total number of integers
 * @param ... List of integer variables to find maximum
 * @return Maximum among all integers passed
 */
int max(int args, ...){
    int i, max, cur;
    va_list valist;
    va_start(valist, args);
    
    max = INT_MIN;
    
    for(i=0; i<args; i++){
        cur = va_arg(valist, int); // Get next elements in the list
        if(max < cur)
            max = cur;
    }
    
    va_end(valist); // Clean memory assigned by valist
    
    return max;
}

/**
 * Allocate one string and write the value of n (>=0) in it. The returned string has strLen characters.
 * @param n: the number > 0
 * @param strLen: the final length of the generated string
 * @return the string or NULL
 */ 
char* intToStr(int n, int strLen){
    if(n<0 || n>=pow(10,strLen)){
        puts("intToStr: n not valid for conversion");
        return NULL;
    }
    int length = snprintf( NULL, 0, "%0*d", strLen, n);
    char* str = malloc( length + 1 );
    if(str==NULL) return NULL;
    snprintf( str, length + 1, "%0*d", strLen, n );
    return str;
}

/**
 * Safe mutex lock
 * @param mux: the mutex to lock
 * @return 
 *      1 if an error occurred
 *      0 otherwise
 */
int safe_mutex_lock(pthread_mutex_t* mux){
    int err;
    if ((err = pthread_mutex_lock(mux)) != 0) {
        errno = err;
        perror("lock");
        return 1;
    }/* else {
        //printf("locked\n");
    }*/
    return 0;
}

/**
 * Unsafe mutex lock
 * @param mux: the mutex to unlock
 * @return 
 *      1 if an error occurred
 *      0 otherwise
 */
int safe_mutex_unlock(pthread_mutex_t* mux){
    int err;
    if ((err = pthread_mutex_unlock(mux)) != 0) {
        errno = err;
        perror("unlock");
        return 1;
    }/* else {
        //printf("unlocked\n");
    }*/
    return 0;
}

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
 * @return 
 *      -1 if error occurred (errno is setted up), 
 *      0 if write returns 0, 
 *      size if it ends up successfully
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
 * @return 
 *      -1 if error occurred (errno is setted up), 
 *      0 if write returns 0, 
 *      1 if it ends up successfully
 */
int sendTo(int fd, char* content, int length){
    msg_t msg;
    msg.len = length;
    char *msgLenAsString;
    if( (msgLenAsString = intToStr(msg.len, 9)) == NULL)
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
    // printf("Invio->");
    // fwrite(msg.content, 1, msg.len, stdout); // debug
    // puts("");
    // fflush(stdout);

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
void getNullMessage(msg_t *msg){
    msg->content = NULL;
    msg->len = 0;
}

/**
 * Build the response for the client
 * @param msgPtr: the pointer to the message struct
 * @param response: the content of the message. Not raw data
 * @return
 *      -1 if fatal error occurred
 *      0 otherwise
 */
int buildMsg(msg_t* msgPtr, char* response){
    if(response == NULL){
        getNullMessage(msgPtr);
        return 0;
    }
    // build response
    ec( msgPtr->content = strdup(response), NULL, return -1 );
    msgPtr->len = strlen(response);
    return 0;
}