#include<filesystem_util.h>
#include<stdlib.h>
#include<string.h>

/*Note: 
creazione/distruzione:
altrimenti acquire globale - cerca file - acquire file - operazione - release file - release globale

write/appendTo:
acquire globale - cerca file - acquire file - release globale - operazione - release file

read:
acquire globale - cerca file - operazione - release globale

*/
/*----------- Util --------------------------------------------------------------------------------*/
/* Print the storage*/
void printFs(){
    /*file_t* currNode = fs.fileListHead;
    while(currNode != NULL){
        printf("%s\n %d\n", currNode->path, currNode->content != NULL);
        currNode = currNode->next;
    }*/
}
/**
 * Look for the file at path
 * @param path: the file path
 * @return the pointer to the file inside the storage or NULL if the file is not in the storage
 */
file_t* searchFile(char* path){
    return icl_hash_find(fs.hFileTable, path);
}

/**
 * Create a new file_t node and store it in file
 * @param filePtr: the pointer to the memory we want to store the new file at
 * @param path: the new file path
 * @return
 *      -1 if fatal error occurred
 *      1 if everything's ok
 */
int buildNewFile(file_t** filePtr, char* path){
    ec( (*filePtr)->path = strdup(path), NULL, return -1 );
    (*filePtr)->content = NULL;
    (*filePtr)->size = 0;
    (*filePtr)->locked_by = NULL;
    (*filePtr)->opened_by = NULL;
    (*filePtr)->writeOwner = -1;
    (*filePtr)->readersCount = 0;
    (*filePtr)->writersCount = 0;
    pthread_cond_init(&((*filePtr)->Go), NULL);
    pthread_mutex_init(&((*filePtr)->mux), NULL);
    pthread_mutex_init(&((*filePtr)->order), NULL);
    (*filePtr)->prev = NULL;
    (*filePtr)->next = NULL;
    return 1;
}

/**
 * Free the whole list of client_t, list
 * @param list: the list to free
 * @param warnClients: flag: warn the clients about the distruction of the file
 */
int freeClientsList(client_t* list, char* filepath, char warnClients){
    client_t* nextNode;
    // in order to warn clients: --------
    int pathSize;
    char* str = NULL;
    int strLen;
    if(filepath != NULL){
        pathSize = strlen(filepath);
        char* pathLenAsString;
        strLen = strlen(LOCKED_FILE_REMOVED) + MSG_LEN_LENGTH + pathSize;
        // Send a message to the client clientFd, warning him the file was removed
        ec( (pathLenAsString = intToStr(pathSize, MSG_LEN_LENGTH)), NULL, return -1 );
        ec( str = calloc(strLen + 1, sizeof(char)), NULL, return -1 );
        if( memcpy(str, LOCKED_FILE_REMOVED, 3) == NULL || 
                memcpy(str+3, pathLenAsString, MSG_LEN_LENGTH) == NULL ||
                memcpy(str+3+MSG_LEN_LENGTH, filepath, pathSize) == NULL ){
            free(pathLenAsString);
            free(str);
            return -1;
        }
        free(pathLenAsString);
    }
    // ----------
    while(list != NULL){
        nextNode = list->next;
        if(warnClients && list->client_fd > 0){
            sendTo(list->client_fd, str, strLen);
        }
        free(list);
        list = nextNode;
    }
    free(str);
    return 0;
}

/**
 * Remove clientFD from the client_t list
 * @param clientFD: the descriptor we are looking for
 * @param list: the list
 * @return
 *      0 if client was removed
 *      1 if client not in list
 */
int removeClientFromList(client_fd_t clientFD, client_t** list){
    client_t* currNode = *list;
    client_t* prevNode = NULL;
    while(currNode != NULL){
        if(clientFD == currNode->client_fd)
            break;
        prevNode = currNode;
        currNode = currNode->next;
    }
    if(currNode != NULL){
        //printf("\t\tclient: %d\n", currNode->client_fd);
        // the client's in list
        if(prevNode == NULL){
            // currNode is the head of the list
            *list = currNode->next;
        }else{
            prevNode->next = currNode->next;
        }
        free(currNode);
        return 0;
    }
    return 1;
}

/**
 * Look for clientFD in the client_t list
 * @param clientFD: the descriptor we are looking for
 * @param list: the list
 * @return
 *      0 if client was found
 *      1 if client not in list
 */
int searchClientInList(client_fd_t clientFD, client_t* list){
    client_t* currNode = list;
    while(currNode != NULL){
        if(clientFD == currNode->client_fd)
            return 0;
        currNode = currNode->next;
    }
    return 1;
}

/**
 * Destroy a file freeing the memory and destroying mutexes and condition variable
 * Warn the clients who are locking the file
 * @param file: the pointer to the file
 * @return the file content
 */
char* destroyFile(file_t* file, char warn_lockers){
    freeClientsList(file->locked_by, file->path, warn_lockers);
    freeClientsList(file->opened_by, 0, 0);
    free(file->path);
    pthread_mutex_destroy(&(file->mux));
    pthread_mutex_destroy(&(file->order));
    pthread_cond_destroy(&(file->Go));
    char* content = file->content;
    free(file);
    return content;
}


/**
 * Insert a new client_t into the listPtr list. Tail insertion 
 * @param listPtr: the pointer to the list
 * @param value: the new client fd
 * @return
 *      -1 if fatal error occurred
 *      0 if everything's ok
 */
int clientListTailInsertion(client_t** listPtr, client_fd_t value){
    client_t* newC = (client_t*) malloc(sizeof(client_t));
    if(newC==NULL) return -1;
    newC->client_fd = value;
    newC->next = NULL;
    if(*listPtr == NULL)
        *listPtr = newC;
    else{
        client_t* currPtr = *listPtr;
        while(currPtr!=NULL){
            if(currPtr->next == NULL)
                break;
            currPtr = currPtr->next;
        }
        currPtr->next = newC;
    }
    return 0;
}

/**
 * Send content to the client clientFd
 * @param clientFd: the client we want to send the message to
 * @param content: the content we are going to send
 * @param contentLength: the length of the content
 * @param TAG: the tag before the content
 * @return
 *      -1 if fatal error occurred
 *      0 if everything's ok
 */
int sendContentToClient(client_fd_t clientFd, char* content, int contentLength, char* TAG){
    if(content == NULL)
        return 0;
    //printf("content:%s", content);
    char* msg;
    int newLen = strlen(TAG) + MSG_LEN_LENGTH + contentLength;
    char* contentLenAsString;
    if( (contentLenAsString = intToStr(contentLength, MSG_LEN_LENGTH)) == NULL)
        return -1;
    ec( msg = calloc(newLen + 1, sizeof(char)), NULL, return -1 );
    if( memcpy(msg, TAG, 3) == NULL || memcpy(msg+3, contentLenAsString, MSG_LEN_LENGTH) == NULL ||
            memcpy(msg+3+MSG_LEN_LENGTH, content, contentLength) == NULL ){
        free(msg);
        free(contentLenAsString);
        return -1;
    }
    sendTo(clientFd, msg, newLen);
    free(msg);
    free(contentLenAsString);
    return 0;
}

/**
 * Append content to the end of the file
 * @param filePtr: the file
 * @param content: the content
 * @param contentSize: the content length
 * @return
 *      -1 if fatal error occurred
 *      0 otherwise
 */
int appendContentToFile(file_t* filePtr, char* content, int contentSize){
    size_t newLen = filePtr->size + contentSize;
    ec( filePtr->content = realloc(filePtr->content, (newLen + 1)*sizeof(char)), 
        NULL, return -1
    );
    memset(filePtr->content + newLen, 0, 1);
    memcpy(filePtr->content + filePtr->size, content, contentSize);
    filePtr->size = newLen;
    
    return 0;
}

/*----------- Middleware -----------------------------------------------------------------------*/
/**
 * Put the client into the openers list of the file referred by file_path.
 * The file is created if it doesn't exist and flags==O_CREATE.
 * The procedure fails with logical error if the file exists and (flags & O_CREATE == 1)
 * or if the file doesn't exist and (flags & O_CREATE == 0)
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param flags: the flags of the open request
 * @param file_path: the path of the file to open
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. creation of an existing file...)
 *      0 otherwise
 */ 
int openFile(msg_t* msgPtr, client_fd_t client_fd, char flags, char* file_path){
    file_t* file;
    LOCK_FS_SEARCH_FILE(1, client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        if( !(flags & O_CREATE) ){
            // O_CREATE is not part of the flags
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 1;
        }
        
        // O_CREATE has been specified
        ec( file = malloc(sizeof(file_t)), NULL, return -1 );

        if( buildNewFile(&file, file_path) == -1 ){
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            return -1;
        }
        
        // insert file performing eviction if needed (maybe we get over the file_capacity limit)
        int fileInsertion = insertFile(file, PERFORM_EVICTION, client_fd);
        if( fileInsertion == -1 ){
            // fatal error
            free( destroyFile(file, 0) );
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            return -1;
        }
        if(fileInsertion == 1){
            // file's too large for this filesystem
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            ec( buildMsg(msgPtr, CONTENT_TOO_LARGE ), -1, return -1 );
            return 1;
        }

        puts("file doesn't exist"); printFs();

        // the new file has been successfully inserted
        START_WRITE(file, fs, 1, return -1);

        if( flags & O_LOCK ){
            // O_LOCK has been specified
            // let's make the new file locked by client_fd
            ec( clientListTailInsertion(&(file->locked_by), client_fd), -1, 
                DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
                return -1 
            );

            // this client is the one who can write on this new file
            file->writeOwner = client_fd;
        }
        
        // let's make the new file opened by client_fd
        ec( clientListTailInsertion(&(file->opened_by), client_fd), -1, 
            DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
            return -1 
        );
        
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if( flags & O_CREATE ){
            // O_CREATE has been specified
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 1;
        }

        puts("file exists"); printFs();

        if(fs.eviction_policy == LRU)
            getFsFileToTail(file);

        // the new file exists
        START_WRITE(file, fs, 1, return -1);
        // is the file locked by someone else?
        if( file->locked_by != NULL && (file->locked_by)->client_fd != client_fd ){
            // file is locked by someone else, operation not permitted
            DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 0;
        }

        // file's locked by no one or locked by the same client who did the request
        // NOTE: we assume a client can lock the file even if he's already a locker
        if( file->locked_by == NULL ){
            if( flags & O_LOCK ){
                // let's make it locked by client_fd
                ec( clientListTailInsertion(&(file->locked_by), client_fd), -1, 
                    DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
                    return -1 
                );
            }
        }

        // NOTE: we assume a client can open the file even if he's already an opener
        if( searchClientInList(client_fd, file->opened_by)!=0 ){
            // let's make it opened by client_fd if he's not an opener yet
            ec( clientListTailInsertion(&(file->opened_by), client_fd), -1, 
                DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
                return -1 
            );
        }
    );

    DONE_WRITE(file, fs, 
        file->writeOwner = (flags & O_LOCK && flags & O_CREATE) ? client_fd : -1;, 
        return -1
    );

    ec( buildMsg(msgPtr, FINE_REQ_RESPONSE), -1, return -1 );
    return 0;
}

/**
 * Close a previously opened file
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param file_path: the path of the file to close
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. closing an unopened file)
 *      0 otherwise
 */ 
int closeFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    file_t* file;
    LOCK_FS_SEARCH_FILE(1, client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, WRONG_FILEPATH_RESPONSE ), -1, return -1 );
        return 1;
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        START_WRITE( file, fs, 1, return -1 );

        // is the file locked by someone else?
        if( file->locked_by != NULL && (file->locked_by)->client_fd != client_fd ){
            // file is locked by someone else, operation not permitted
            DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 0;
        }

        // file's locked by no one or locked by the same client who did the request

        // update the writeOwner
        file->writeOwner = -1;

        // does client_fd previously opened the file?
        if( removeClientFromList(client_fd, &(file->opened_by)) == 0 ){
            // client was removed
            ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, 
                DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
                return -1 
            );
        }else{
            // client not in list
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, 
                DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
                return -1 
            );
        }
        DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
    );
    return 0;
}

/**
 * Read a file
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param file_path: the path of the file to read
 * @param isReadN: flag for readN requests
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. reading an unopened file while isReadN==0)
 *      0 otherwise
 */ 
int readFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path, char isReadN){
    file_t* file;
    LOCK_FS_SEARCH_FILE(!isReadN, client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, WRONG_FILEPATH_RESPONSE ), -1, return -1 );
        return 1;

        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if(fs.eviction_policy == LRU && !isReadN) // random read doesn't count
            getFsFileToTail(file);

        START_READ( file, fs, !isReadN, return -1 );

        // is the file locked by someone else?
        if( (file->locked_by != NULL && (file->locked_by)->client_fd != client_fd)){
            // file is locked by someone else
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            if(!isReadN) ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            return 1;
        }

        // if the file is not opened by the client AND this is not a readN request
        if( searchClientInList(client_fd, file->opened_by)!=0 && !isReadN ){
            // client is not an opener 
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            DONE_READ(file, fs, file->writeOwner = -1;, 
                if(!isReadN) ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1
            );
            if(!isReadN) ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            return 1;
        }

        // the client previously opened the file and is, maybe, the locker OR this is a readN request
        
        if(file->content == NULL){
            ec( buildMsg(msgPtr, NO_CONTENT_FILE ), -1, return -1 );
            DONE_READ(file, fs, file->writeOwner = -1;, 
                if(!isReadN) ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1
            );
            if(!isReadN) ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            return 1;
        }
        char* str;
        int newLen = strlen(READ_FILE_CONTENT) + MSG_LEN_LENGTH + file->size;
        char* contentLenAsString;
        if( (contentLenAsString = intToStr(file->size, MSG_LEN_LENGTH)) == NULL){
            if(!isReadN) ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            return -1;
        }
        ec( str = calloc(newLen + 1, sizeof(char)), NULL, return -1 );

        if( memcpy(str, READ_FILE_CONTENT, 3) == NULL || 
                memcpy(str+3, contentLenAsString, MSG_LEN_LENGTH) == NULL ||
                memcpy(str+3+MSG_LEN_LENGTH, file->content, file->size) == NULL ){
            free(contentLenAsString);
            free(str);
            if(!isReadN) ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            return -1;
        }

        free(contentLenAsString);
        msgPtr->content = str;
        msgPtr->len = newLen;
        //printf("len:%d\ncontent:%s\n", msgPtr->len, msgPtr->content);
        DONE_READ(file, fs, file->writeOwner = -1;, return -1);
    );
    return 0;
}


/**
 * Read n different files
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param N: the number of files to read (if 0, read the whole storage)
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. appending to an unopened file)
 *      0 otherwise
 */ 
int readNFiles(msg_t* msgPtr, client_fd_t client_fd, int N){
    ec( SAFE_LOCK(fs.mux), 1, return -1 );
    if(N <= 0 || N > fs.currFilesNumber )
        N = fs.currFilesNumber;
    ec( buildMsg(msgPtr, NULL ), -1, ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1 );
    file_t *currFile = fs.fileListHead;
    char* filePathLengthAsString = NULL;
    int readOpRes = 0;
    while(readOpRes!=-1 && currFile != NULL && N > 0){
        ec( buildMsg(msgPtr, NULL ), -1, ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1 );
        if( (readOpRes=readFile(msgPtr, client_fd, currFile->path, 1)) == 0){
            // the file was read -> content info in msgPtr
            // send the path too. format here is: RRRLLLLLLLLLcontent
            //filePath length
            if( (filePathLengthAsString = intToStr(strlen(currFile->path), MSG_LEN_LENGTH)) == NULL){
                ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
                return -1;
            }
            // append the path length and the path
            ec(
                msgPtr->content = realloc(msgPtr->content, 
                        msgPtr->len + MSG_LEN_LENGTH + strlen(currFile->path)+1),
                NULL, ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1
            );
            memmove(msgPtr->content + msgPtr->len, filePathLengthAsString, MSG_LEN_LENGTH);
            memmove(msgPtr->content + msgPtr->len + MSG_LEN_LENGTH, 
                currFile->path, strlen(currFile->path)+1); // move the '\0' too
            msgPtr->len += MSG_LEN_LENGTH + strlen(currFile->path);
            // (new) format here is: RRRLLLLLLLLLcontentLLLLLLLLLfilepath
            //printf("len:%d\ncontent:%s\n", msgPtr->len, msgPtr->content);
            sendTo(client_fd, msgPtr->content, msgPtr->len);
            free(filePathLengthAsString);
            filePathLengthAsString=NULL;
            N--;
        }//else null-content-file (not sent back)
        free(msgPtr->content);
        currFile = currFile->next;
    }
    // end of req
    ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
    ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );
    return 0;
}

/**
 * Write the file content
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param content: the content to write to the file
 * @param contentSize: the content length
 * @param file_path: the path of the file
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. appending to an unopened file)
 *      0 otherwise
 */ 
int writeFile(msg_t* msgPtr, client_fd_t client_fd, 
        char* content, int contentSize, char* file_path){
    file_t* file;
    LOCK_FS_SEARCH_FILE(1, client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
        return 1;
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if(client_fd != file->writeOwner){
            file->writeOwner = -1;
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 1;
        }

        assert(
            (file->locked_by)->client_fd == client_fd && searchClientInList(client_fd, file->opened_by)==0
        );
        
        if(fs.eviction_policy == LRU)
            getFsFileToTail(file);

        /* compare new file size and fs capacity */
        if(contentSize > fs.byte_capacity){
            /* no way we can write this content to the file. Rejected*/
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            ec( buildMsg(msgPtr, CONTENT_TOO_LARGE ), -1, return -1 );
            return 1;
        }
        
        // compare new storage size and capacity
        int newFsSize = contentSize + fs.currSize;
        if(newFsSize > fs.byte_capacity){
            // eviction needed
            ec( evictFiles(newFsSize - fs.byte_capacity, -1,
                client_fd), -1, return -1 );
        }

        // don't unlock the store here because of readNfiles handling: 
        // deadlock if some N files reader locks the store
        START_WRITE( file, fs, 0, 
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); 
            return -1;
        );
        // NOTE: UNLOCK(fs.mux) NEEDED

        ec( appendContentToFile(file, content, contentSize), -1, 
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1 
        );

        /* update size */
        fs.currSize += file->size;
        SET_MAX(fs.maxReachedSize, fs.currSize);

        ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, 
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1 
        );

        DONE_WRITE(file, fs, file->writeOwner = -1;, 
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1
        );
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
    );
    return 0;
}

/**
 * Append content to a file
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param content: the content to append to the file
 * @param contentSize: the content length
 * @param file_path: the path of the file
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. appending to an unopened file)
 *      0 otherwise
 */ 
int appendToFile(msg_t* msgPtr, client_fd_t client_fd, 
        char* content, int contentSize, char* file_path){
    file_t* file;
    LOCK_FS_SEARCH_FILE(1, client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, WRONG_FILEPATH_RESPONSE ), -1, return -1 );
        return 1;
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if(fs.eviction_policy == LRU)
            getFsFileToTail(file);
        
        int newFsSize = contentSize + fs.currSize;
        int newFileSize = contentSize + file->size;

        /* compare new file size and fs capacity */
        if(newFileSize > fs.byte_capacity){
            /* no way we can append this content to the file. Rejected*/
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            ec( buildMsg(msgPtr, CONTENT_TOO_LARGE ), -1, return -1 );
            return 1;
        }
        
        // don't unlock the store here
        START_WRITE( file, fs, 0, 
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1 
        );
        // NOTE: UNLOCK(fs.mux) NEEDED

        // is the file locked by someone else? OR is the client opened by client_fd?
        if( (file->locked_by != NULL && (file->locked_by)->client_fd != client_fd) || 
                searchClientInList(client_fd, file->opened_by)!=0 ){
            // file is locked by someone else
            DONE_WRITE(file, fs, file->writeOwner = -1;, 
                ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
                return -1
            );
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 1;
        }
        // the client previously opened the file and is, maybe, the locker

        // compare new storage size and capacity
        if(newFsSize > fs.byte_capacity){
            // eviction needed
            ec( evictFiles(newFsSize - fs.byte_capacity, -1, client_fd), -1, 
                DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
                ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
                return -1 
            );
        }
        
        ec( appendContentToFile(file, content, contentSize), -1, 
            DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
            return -1 
        );

        DONE_WRITE(file, fs, file->writeOwner = -1;, 
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            return -1
        );
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );
    );
    return 0;
}

/**
 * Lock a file
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param file_path: the path of the file to lock
 * @return 
 *      -1 if fatal error occurred
 *      0 otherwise
 */ 
int lockFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    file_t* file;
    LOCK_FS_SEARCH_FILE(1, client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, WRONG_FILEPATH_RESPONSE ), -1, return -1 );
        return 1;
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if(fs.eviction_policy == LRU)
            getFsFileToTail(file);
        
        START_WRITE( file, fs, 1, return -1 );

        // is the file locked by someone else?
        if( file->locked_by != NULL && (file->locked_by)->client_fd != client_fd ){
            // file is locked by someone else, push this client in locked_by list
            ec( clientListTailInsertion(&(file->locked_by), client_fd), -1, 
                DONE_WRITE(file, fs, file->writeOwner = -1;, return -1); 
                return -1 
            );
            DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
            ec( buildMsg(msgPtr, NULL ), -1, return -1 );
            return 0;
        }else{
            // file's locked by no one or locked by the same client who did the request
            // NOTE: specific says the operation ends successfully even if the client already
            // holds the lock
            if( file->locked_by == NULL )
                ec( clientListTailInsertion(&(file->locked_by), client_fd), -1,
                    DONE_WRITE(file, fs, file->writeOwner = -1;, return -1); 
                    return -1 
                );
        }
        DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
        ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );
    );
    return 0;
}

/**
 * Unlock a file
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param file_path: the path of the file to unlock
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. unlocking a not locked-by-me file)
 *      0 otherwise
 */ 
int unlockFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    file_t* file;
    LOCK_FS_SEARCH_FILE(1, client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, WRONG_FILEPATH_RESPONSE ), -1, return -1 );
        return 1;
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if(fs.eviction_policy == LRU)
            getFsFileToTail(file);
        
        START_WRITE( file, fs, 1, return -1 );

        // is the file locked by someone else?
        if( file->locked_by == NULL || 
                (file->locked_by != NULL && (file->locked_by)->client_fd != client_fd) ){
            // file is locked by someone else or not locked
            DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 1;
        }else{
            // file's locked by the same client who did the request
            removeClientFromList(client_fd, &(file->locked_by));
            DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
            ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );
        }
    );
    return 0;
}

/**
 * Remove a file
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param file_path: the path of the file to remove
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. removing a not existing file )
 *      0 otherwise
 */ 
int removeFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    file_t* file;
    LOCK_FS_SEARCH_FILE(1, client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, WRONG_FILEPATH_RESPONSE ), -1, return -1 );
        return 1;
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if(fs.eviction_policy == LRU)
            getFsFileToTail(file);
        
        START_WRITE( file, fs, 0, return -1 );
        // NOTE: UNLOCK(fs.mux) NEEDED

        // is the file locked by someone else?
        if( file->locked_by == NULL || 
                (file->locked_by != NULL && (file->locked_by)->client_fd != client_fd) ){
            // file is locked by someone else or not locked
            DONE_WRITE(file, fs, file->writeOwner = -1;, 
                ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
                return -1
            );
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 1;
        }else{
            // file's locked by the same client who did the request
            deleteFile(file, client_fd, 0);
            // a LOCKED_FILE_REMOVED message has been already 
            // sent to the client by deleteFile function. Nothing more to do
        }
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
    );
    return 0;
}
 


/*----------- Backend --------------------------------------------------------------------------*/
/**
 * Initialize the filesystem structure
 * @param file_capacity: max number of stored files
 * @param byte_capacity: max number of bytes used by the storage
 * @param eviction_policy: the eviction policy of the filesystem
 * @return
 *      -1 if the creation of the hash table fails or any value is not valid
 *      0 otherwise
 */
int initFileSystem(size_t file_capacity, size_t byte_capacity, char eviction_policy){
    fs.fileListHead = NULL;
    fs.fileListTail = NULL;
    /* init hash table */
    ec( fs.hFileTable = icl_hash_create(file_capacity, NULL, NULL), 
        NULL, return -1
    );
    /* check values */
    if(file_capacity<=0 || byte_capacity<=0 || (eviction_policy!=FIFO && eviction_policy!=LRU)){
        /* destroy hash table */
        icl_hash_destroy(fs.hFileTable,free,NULL);
        return -1;
    }
    fs.file_capacity = file_capacity;
    fs.byte_capacity = byte_capacity;
    fs.eviction_policy = eviction_policy;
    fs.currFilesNumber = 0;
    fs.currSize = 0;
    fs.maxReachedFilesNumber = 0;
    fs.maxReachedSize = 0;
    fs.performed_evictions = 0;
    if( pthread_mutex_init(&(fs.mux), NULL) != 0 ){
        icl_hash_destroy(fs.hFileTable, free, NULL);
        return -1;
    }
    return 0;
}

/**
 * Free the whole list of file_t list
 * @param the list to free
 */
void freeFilesList(file_t* list){
    puts("\n\nDestroying file system.");
    printf("Highest number of files ever reached: %ld\n", fs.maxReachedFilesNumber);
    printf("Highest number of stored bytes ever reached: %ld\n", fs.maxReachedSize);
    printf("Times the eviction algorithm has been called up: %ld\n", fs.performed_evictions);
    printf("Files at the end of this session:\n");
    file_t* nextNode;
    while(list != NULL){
        nextNode = list->next;
        printf("%s\t\n", list->path);
        free( destroyFile(list, 0) ); // it returns the content of the file. We free it too
        list = nextNode;
    }
    puts("\n");
}

/**
 * Destroy the filesystem freeing the memory
 */
void destroyFileSystem(){
    // destroy hash table, no free functions here because the keys are inside the files list
    icl_hash_destroy(fs.hFileTable,NULL,NULL);
    freeFilesList(fs.fileListHead);
}

/**
 * Update the lockers list of the files inside the storage removing clientFD
 * @param clientFD: the client desctiptor
 * @return 
 *      0 success
 *      -1 fatal error
 */
int updateLockersList(client_fd_t clientFD){
    ec( SAFE_LOCK(fs.mux), 1, return -1 );
    file_t* nextNode = fs.fileListHead;
    msg_t msg;
    ec( buildMsg(&msg, NULL ), -1, return -1 ); // init msg.content to NULL 
    while(nextNode != NULL){
        START_WRITE( nextNode, fs, 0, 
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1 
        ); 
        if(searchClientInList(clientFD, nextNode->locked_by) == 0){
            if((nextNode->locked_by)->client_fd == clientFD){
                // warn the 2nd locker and remove the first one
                // remove the client
                removeClientFromList(clientFD, &(nextNode->locked_by));
                // warn the current locker
                if( buildMsg(&msg, FINE_REQ_RESPONSE ) == -1 &&
                        sendTo(clientFD, msg.content, msg.len)!=1 ){
                    DONE_WRITE(nextNode, fs, , 
                        ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1
                    );
                    return -1;
                }
            }else{
                // remove the client
                removeClientFromList(clientFD, &(nextNode->locked_by));
            }
        }
        DONE_WRITE(nextNode, fs, , 
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; ); return -1
        );
        nextNode = nextNode->next;
    }
    free(msg.content);
    ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
    return 0;
}

/**
 * Detach a file from the storage
 * @param filePtr: the pointer to the file
 */
void detachFileFromStorage(file_t* filePtr){
    if( filePtr->prev == NULL ){
        // this is the head of the list
        if( filePtr->next != NULL)
            (filePtr->next)->prev = NULL;
        else{ 
            //filePtr is the tail too
            fs.fileListTail = NULL;
        }
        fs.fileListHead = filePtr->next;
    }else{
        // not the head of the list
        if(filePtr->next != NULL)
            (filePtr->next)->prev = filePtr->prev;
        else{
            //filePtr is the tail
            (filePtr->prev)->next = NULL; // for sure filePtr->prev exists here
            fs.fileListTail = filePtr->prev;
        }
        (filePtr->prev)->next = filePtr->next;
    }
}

/**
 * Push a file to the storage tail
 * @param filePtr: the pointer to the file
 */
void pushFileToStorageTail(file_t* filePtr){
    if( fs.fileListHead == NULL ){
        /* storage is empty */
        fs.fileListHead = filePtr;
        fs.fileListTail = fs.fileListHead;
        filePtr->next = NULL;
        filePtr->prev = NULL;
    }else{
        (fs.fileListTail)->next = filePtr;
        filePtr->prev = fs.fileListTail;
        filePtr->next = NULL;
        fs.fileListTail = filePtr;
    }
}

/**
 * Move the file to the end of the storage (to the tail)
 * @param filePtr: the pointer to the file
 * @return 
 *      -1 if fatal error occurred
 *      0 if everything's ok
 */
int getFsFileToTail(file_t* filePtr){
    if(filePtr == NULL)
        return -1;
    // detach the file from the storage
    detachFileFromStorage(filePtr);
    // insert the file to the tail
    pushFileToStorageTail(filePtr);
    return 0;
}

/**
 * Insert a file into the file list. A tail insertion is performed.
 * @param file: the file
 * @param performEviction: perform the eviction flag
 * @param client_fd: the client who asked for insertion (only if performEvicion != NO_EVICTION)
 * @return 
 *      -1 got a fatal error
 *      1 if insertion is not permitted (file too large)
 *      0 otherwise
 */
int insertFile(file_t* filePtr, char performEviction, client_fd_t client_fd){
    char eviction_needed = 0;
    /* compare file size and fs capacity */
    if(filePtr->size > fs.byte_capacity)
        /* no way we can insert this file into the storage. Rejected*/
        return 1;
    
    /* check new sizes */
    int newSize = filePtr->size + fs.currSize;
    int newFilesNum = fs.currFilesNumber + 1;
    if(newSize > fs.byte_capacity || newFilesNum > fs.file_capacity)
        eviction_needed = 1;

    if( fs.currFilesNumber != 0 ){
        /* storage is not empty */
        if( eviction_needed ){
            if(performEviction){
                ec( evictFiles(newSize - fs.byte_capacity, newFilesNum - fs.file_capacity, client_fd),
                    -1, return -1 
                );
            }else{
                //got a serious problem here
                return -1;
            }
        }
    }

    // insert the file to the tail
    pushFileToStorageTail(filePtr);

    /* we inserted the file here, let's put it in the hash table and update the size values */
    /* update size */
    fs.currSize += filePtr->size;
    SET_MAX(fs.maxReachedSize, fs.currSize);

    /* update current files number */
    fs.currFilesNumber++;
    SET_MAX(fs.maxReachedFilesNumber, fs.currFilesNumber);
    
    //printf("\tcurrFilesNumber: %ld\n", fs.currFilesNumber);

    ec( icl_hash_insert(fs.hFileTable, filePtr->path, filePtr), NULL, return -1 );

    return 0;
}

/**
 * Build a list of files which are the next victims of the eviction policy
 * @param bytesNum: the number of bytes we need to free
 * @param filesNum: the number of files we need to evict
 * @return 
 *      -1 if fatal error occurred
 *      0 otherwise
 */
int evictFiles(int bytesNum, int filesNum, client_fd_t clientFd){
    fs.performed_evictions++;
    file_t* currFile = fs.fileListHead;
    file_t* nextFile;
    while(currFile != NULL){
        nextFile = currFile->next;
        if( filesNum > 0 || bytesNum > 0){
            // we need to evict someone
            if( fs.eviction_policy == FIFO || fs.eviction_policy == LRU ){
                // we evict the less recently inserted files (from fs.fileListHead ahead)
                START_WRITE(currFile, fs, 0, return -1);
                filesNum--;
                bytesNum -= currFile->size;
                ec( deleteFile(currFile, clientFd, 1), -1, return -1 );
            }
        }
        currFile = nextFile;
    }

    //assert(filesNum > 0 || bytesNum > 0 );
    return 0;
}


/**
 * Delete a file
 * @param filePtr: the pointer to the file
 * @param clientFd: the client who triggered the deletion
 * @param isEviction: if it's due to eviction then we want to send the content back to clientFd
 * @return
 *      -1 if fatal error
 *      0 otherwise
 */
int deleteFile(file_t* filePtr, client_fd_t clientFd, char isEviction){
    if( filePtr == NULL)
        return 0;

    // detach file from the storage
    detachFileFromStorage(filePtr);
    icl_hash_delete(fs.hFileTable, filePtr->path, NULL, NULL);
    fs.currFilesNumber--;
    fs.currSize -= filePtr->size;

    int tempContentSize = filePtr->size;
    char* filePath;
    ec( filePath = strdup(filePtr->path), NULL, return -1);
    char *tempContent = destroyFile(filePtr, 1);
    if( isEviction ){
        char* str;
        int newLen = strlen(EVICTED_FILE_CONTENT) + MSG_LEN_LENGTH + tempContentSize;
        char* contentLenAsString;
        ec( (contentLenAsString = intToStr(tempContentSize, MSG_LEN_LENGTH)), NULL, return -1 );
        ec( str = calloc(newLen + 1, sizeof(char)), NULL, return -1 );
        if( memcpy(str, EVICTED_FILE_CONTENT, 3) == NULL || 
                memcpy(str+3, contentLenAsString, MSG_LEN_LENGTH) == NULL ||
                memcpy(str+3+MSG_LEN_LENGTH, tempContent, tempContentSize) == NULL ){
            free(contentLenAsString);
            free(str);
            return -1;
        }
        free(contentLenAsString);
        free(tempContent);
        tempContent = str;
        tempContentSize = newLen;

        // send the path too. format here is: RRRLLLLLLLLLcontent
        //filePath length
        char* filePathLengthAsString;
        ec( (filePathLengthAsString = intToStr(strlen(filePath), MSG_LEN_LENGTH)), NULL, return -1 );
        // append the path length and the path
        ec(
            tempContent = realloc(tempContent, 
                    tempContentSize + MSG_LEN_LENGTH + strlen(filePath)+1),
            NULL, return -1
        );
        memmove(tempContent + tempContentSize, filePathLengthAsString, MSG_LEN_LENGTH);
        memmove(tempContent + tempContentSize + MSG_LEN_LENGTH, 
            filePath, strlen(filePath)+1); // move the '\0' too
        tempContentSize += MSG_LEN_LENGTH + strlen(filePath);
        // (new) format here is: RRRLLLLLLLLLcontentLLLLLLLLLfilepath
        //printf("len:%d\ncontent:%s\n", tempContentSize, tempContent);
        sendTo(clientFd, tempContent, tempContentSize);
        free(filePathLengthAsString);
    }else{
        // removing file
        msg_t msg;
        ec( buildMsg(&msg, FINE_REQ_RESPONSE), -1, return -1 );
        sendTo(clientFd, msg.content, msg.len);
        free(msg.content);
    }
    free(tempContent);
    free(filePath);
    return 0;
}