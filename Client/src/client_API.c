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

/*---------- UTIL -----------------------------------------------------------------------------------*/

/**
 * Get content+filename from a received message
 */
int getContentAndFilename(char* response, char* operation, char** content, size_t* contentSize, char** fileName){
    // response format: LLLLLLLLLcontentLLLLLLLLLfilepath
    char* temp = NULL;
    // get content size
    if( (temp = strndup(response, MSG_LEN_LENGTH)) == NULL) return -1;
    if( isSizeT(temp, contentSize) != 0 || *contentSize < 0){
        free(temp);
        return -1;
    }
    free(temp);
    temp = NULL;
    
    // get filename size
    size_t path_size;
    if( (temp = strndup(response+MSG_LEN_LENGTH+(*contentSize), MSG_LEN_LENGTH)) == NULL) return -1;
    if( isSizeT(temp, &path_size) != 0 || path_size < 0){
        free(temp);
        return -1;
    }
    free(temp);
    temp = NULL;

    // get fileContent
    ec( *content = calloc(*contentSize+1, sizeof(char)), NULL, return -1 );
    memmove(*content, response+MSG_LEN_LENGTH, *contentSize);

    //get filename
    char* lastSlashPtr = NULL;
    if((lastSlashPtr = strrchr( response+MSG_LEN_LENGTH+(*contentSize)+MSG_LEN_LENGTH, '/' )) != NULL){
        // filename will be the last part of the path
        *fileName = lastSlashPtr+1; // no free needed
    }else{
        *fileName = response+MSG_LEN_LENGTH+(*contentSize)+MSG_LEN_LENGTH; // no free needed
    }

    if(stdout_print){ PRINT_INFO(operation, *fileName, *contentSize); }

    // debug
    // printf("info file: %ld\n", *contentSize);
    // fwrite(*content, 1, 20, stdout);
    // puts("");
    // fflush(stdout);
    return 0;
}

/**
 * Get server response(s)
 * @param removing_the_file: this flag is set if the client did a removeFile request
 * @param buf: the buffer we want to store the read file content to
 * @param content_size: the read content size
 * @return
 *      0 if we got a fine response
 *      -1 error occurred (errno is setted up)
 */
int getServerResponse(const char* req_related_filepath, char attemptingToLock, // params for lock related stuff
            void** buf, size_t* content_size, // params for readFile
            char isReadN, const char* readNDir, // params for readNFiles
            const char* eviction_dir){ // directory for evicted files
    char *response = NULL;
    int n;
    size_t cSize;
    int retVal = -1;
    char* temp = NULL, *content = NULL, *filename = NULL;
    errno = 0;
    while(1){
        n = getServerMessage(sockfd, &response);
        if(n!=1)
            break;

        if(strncmp(response, EVICTED_FILE_CONTENT, 3)==0){
            // procedure for a performed eviction
            // response format: RRRLLLLLLLLLcontentLLLLLLLLLfilepath
            
            if(eviction_dir != NULL){
                if( getContentAndFilename(response+3, "Eviction", &content, &cSize, &filename) == -1) return -1;
                // store the new file into the directory
                //printf("eviction_dir: %s\n", eviction_dir);
                ec( getAbsolutePath(eviction_dir, &temp), 1, return EXIT_FAILURE; );
                //printf("temp: %s\n", temp);
                ec( getFilePath(&temp, filename), 1, free(temp);return EXIT_FAILURE; );
                //printf("temp: %s\n", temp);
                ec( writeLocalFile(temp, content, cSize), -1, free(temp);return EXIT_FAILURE; );
                if(stdout_print){ PRINT_INFO("Storing", temp, cSize); }
                free(temp);
                free(content);
                content = NULL;
                temp = NULL;
            }// else file was removed
            free(response);
            continue;
        }

        if(strncmp(response, READ_FILE_CONTENT, 3)==0 && isReadN){
            // procedure for a readN request
            // response format: RRRLLLLLLLLLcontentLLLLLLLLLfilepath
            if( getContentAndFilename(response+3, "Reading", &content, &cSize, &filename) == -1) return -1;
            
            if(readNDir != NULL){
                // store the new file into the directory
                ec( getAbsolutePath(readNDir, &temp), 1, return EXIT_FAILURE; );
                ec( getFilePath(&temp, filename), 1, free(temp);return EXIT_FAILURE; );
                ec( writeLocalFile(temp, content, cSize), -1, free(temp);return EXIT_FAILURE; );
                if(stdout_print){ PRINT_INFO("Storing", temp, cSize); }
                free(temp);
                temp = NULL;
            }else{
                fwrite(content, 1, 20, stdout);
                puts("");
                fflush(stdout);
            }
            free(response);
            free(content);
            content = NULL;
            continue;
        }

        if(strncmp(response, LOCKED_FILE_REMOVED, 3)==0){
            // i didn't remove the file intentionally (using removeFile), instead
            // i was locking or trying to lock the just removed file (eviction triggered by me or other client)
            if( (temp = strndup(response+3, MSG_LEN_LENGTH)) == NULL) return -1;
            if( isSizeT(temp, &cSize) != 0 || cSize < 0){
                free(temp);
                return -1;
            }
            free(temp);
            temp = NULL;
            
            printf("Locked file was removed: %s\n", response+3+MSG_LEN_LENGTH);
            free(response);
            response = NULL;
            if(attemptingToLock && req_related_filepath!=NULL && strncmp(req_related_filepath, response+3+MSG_LEN_LENGTH, cSize) == 0){
                // i was waiting for a result. break here
                break;
            }else{
                // i had the lock, i was not waiting for a result. Continue
                continue;
            }
        }

        if(strncmp(response, READ_FILE_CONTENT, 3)==0 && !isReadN){
            // procedure for a read request
            //printf("Read file content:\n%s\n", response+3+MSG_LEN_LENGTH);
            
            if( (temp = strndup(response+3, MSG_LEN_LENGTH)) == NULL) return -1;
            if( isSizeT(temp, content_size) != 0 || *content_size < 0){
                free(temp);
                return -1;
            }
            free(temp);
            temp = NULL;
            
            // store the content into buf
            ec( *buf = calloc((*content_size)+1, sizeof(char)), NULL, return -1; );
            memcpy(*buf, response+3+MSG_LEN_LENGTH, *content_size);
            retVal = 0; // operation successfully completed

        }else if(strncmp(response, NO_CONTENT_FILE, 3)==0){

        }else if(strncmp(response, FINE_REQ_RESPONSE, 3)==0)
            retVal = 0;
        else if(strncmp(response, WRONG_FILEPATH_RESPONSE, 3)==0)
            errno = ENOENT; // No such file or directory (POSIX.1-2001).
        else if(strncmp(response, NOT_PERMITTED_ACTION_RESPONSE, 3)==0)
            errno = EPERM; // Operation not permitted (POSIX.1-2001).
        else if(strncmp(response, CONTENT_TOO_LARGE, 3)==0)
            errno = EFBIG; // File too large (POSIX.1-2001).
        free(response);
        break;
    }
    return retVal;
}

/*---------- API ------------------------------------------------------------------------------------*/
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
    while ( (res=connect(sockfd,(struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1 && 
            remaining_ms > 0) { // keep trying
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
    errno = 0;
    msg_t msg = buildMessage(OPEN, flags, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    free(msg.content);
    int res = getServerResponse(pathname, flags&O_LOCK, NULL, 0, 0, NULL, NULL);
    if(stdout_print){ PRINT_INFO("Opening", pathname, (long)0); }
    return res;
}

/**
 * Send a close-file request to the server. Any action on the 
 * pathname file after the closeFile fails.
 * @param pathname: the path of the file on the server
 * @return: 0 if file is closed, -1 if we fail (errno is setted up)
 */
int closeFile(const char* pathname){
    errno = 0;
    msg_t msg = buildMessage(CLOSE, NO_FLAGS, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    free(msg.content);

    int res = getServerResponse(pathname, 0, NULL, 0, 0, NULL, NULL);
    if(stdout_print){ PRINT_INFO("Closing", pathname, (long)0); }
    return res;
}

/**
 * Send a lock-file request to the server.
 * @param pathname: the path of the file on the server
 * @return: 0 if file is locked or the request is pending, -1 if we fail (errno is setted up)
 */
int lockFile(const char* pathname){
    errno = 0;
    msg_t msg = buildMessage(LOCK, NO_FLAGS, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    free(msg.content);
    int res = getServerResponse(pathname, 1, NULL, 0, 0, NULL, NULL);
    if(stdout_print){ PRINT_INFO("Locking", pathname, (long)0); }
    return res;
}

/**
 * Send an unlock-file request to the server.
 * @param pathname: the path of the file on the server
 * @return: 0 if file is unlocked, -1 if we fail (errno is setted up)
 */
int unlockFile(const char* pathname){
    errno = 0;
    msg_t msg = buildMessage(UNLOCK, NO_FLAGS, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    free(msg.content);

    int res = getServerResponse(pathname, 0, NULL, 0, 0, NULL, NULL);
    if(stdout_print){ PRINT_INFO("Unlocking", pathname, (long)0); }
    return res;
}

/**
 * Send a remove-file request to the server.
 * @param pathname: the path of the file on the server
 * @return: 0 if file is removed, -1 if we fail (errno is setted up)
 */
int removeFile(const char* pathname){
    errno = 0;
    msg_t msg = buildMessage(REMOVE, NO_FLAGS, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    free(msg.content);
    int res = getServerResponse(pathname, 0, NULL, 0, 0, NULL, NULL);
    if(stdout_print){ PRINT_INFO("Removing", pathname, (long)0); }
    return res;
}

/**
 * Send a read-file request to the server.
 * @param pathname: the path of the file on the server
 * @param buf: it will be the pointer to an heap area where the file content is stored
 * @param size: the size of the buf area
 * @return: 0 if file is read, -1 if we fail (errno is setted up)
 */
int readFile(const char* pathname, void** buf, size_t* size){
    errno = 0;
    msg_t msg = buildMessage(READ, NO_FLAGS, pathname, NULL, 0);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    free(msg.content);
    
    int res = getServerResponse(pathname, 0, buf, size, 0, NULL, NULL);
    if(stdout_print){ PRINT_INFO("Reading", pathname, *size); }
    return res;
}

/**
 * Send a read-N-files request to the server. Store the files into dirname directory.
 * @param N: the number of files to read
 * @param dirname: the path of the directory we are going to store the read files into
 * @return: 0 if files are read, -1 if we fail (errno is setted up)
 */
int readNFiles(int N, const char* dirname){
    errno = 0;
    // request content is the number of files here
    char* N_str = intToStr(N, MSG_LEN_LENGTH);
    msg_t msg = buildMessage(READ_N, NO_FLAGS, NULL, N_str, MSG_LEN_LENGTH);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    free(msg.content);
    free(N_str);    
    int res = getServerResponse(NULL, 0, NULL, 0, 1, dirname, NULL);
    if(stdout_print){ PRINT_INFO("Reading N files", "", (long)0); }
    return res;
}

/**
 * Send a write file request to the server.
 * @param pathname: the path of the file
 * @param dirname: the path of the directory we are going to store the eventually evicted files in
 * @return: 0 if file was written, -1 if we fail (errno is setted up)
 */
int writeFile(const char* pathname, const char* dirname){
    //printf("\t\tdir: %s\n", dirname);
    errno = 0;
    char* fileContent;
    size_t contentSize;
    if( getFileContent(pathname, &fileContent, &contentSize) == -1 ) return -1; // errno is already setted

    msg_t msg = buildMessage(WRITE, NO_FLAGS, pathname, fileContent, contentSize);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        free(fileContent);
        return -1;
    }
    free(msg.content);
    free(fileContent);
    int res = getServerResponse(pathname, 0, NULL, 0, 0, NULL, dirname);
    if(stdout_print){ PRINT_INFO("Writing", pathname, contentSize); }
    return res;
}


int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
    errno = 0;
    msg_t msg = buildMessage(APPEND, NO_FLAGS, pathname, buf, size);
    if( sendTo(sockfd, msg.content, msg.len) != 1){
        free(msg.content);
        return -1;
    }
    free(msg.content);
    int res = getServerResponse(pathname, 0, NULL, 0, 0, NULL, dirname);
    if(stdout_print){ PRINT_INFO("Appending", pathname, size); }
    return res;
}