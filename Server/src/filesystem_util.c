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
    file_t* currNode = fs.fileListHead;
    while(currNode != NULL){
        printf("%s\n %s\n\n", currNode->path, currNode->content);
        currNode = currNode->next;
    }
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
void freeClientsList(client_t* list, char warnClients){
    client_t* nextNode;
    while(list != NULL){
        nextNode = list->next;
        if(warnClients && list->client_fd > 0)
            // Send a message to the client clientFd, warning him the file was removed
            sendTo(list->client_fd, LOCKED_FILE_REMOVED, strlen(LOCKED_FILE_REMOVED));
        free(list);
        list = nextNode;
    }
}

/**
 * Remove clientFD from the client_t list
 * @param clientFD: the descriptor we are looking for
 * @param list: the list
 * @return
 *      0 if client was removed
 *      1 if client not in list
 */
int removeClientFromList(client_fd_t clientFD, client_t* list){
    client_t* currNode = list;
    client_t* prevNode = NULL;
    while(currNode != NULL){
        if(clientFD == currNode->client_fd)
            break;
        prevNode = currNode;
        currNode = currNode->next;
    }
    if(currNode != NULL){
        // the client's in list
        prevNode->next = currNode->next;
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
    freeClientsList(file->locked_by, warn_lockers);
    freeClientsList(file->opened_by, 0);
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
    char* str;
    int newLen = strlen(TAG)+ contentLength;
    ec( str = calloc(newLen + 1, sizeof(char)), NULL, return -1 );
    if( memcpy(str, TAG, 3) == NULL || memcpy(str+3, content, contentLength) == NULL ){
        free(str);
        return -1;
    }
    sendTo(clientFd, str, newLen);
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
    ec( filePtr->content = realloc(filePtr->content, (filePtr->size + contentSize + 1)*sizeof(char)), 
        NULL, return -1
    );
    memcpy(filePtr->content + filePtr->size, content, contentSize);
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
    LOCK_FS_SEARCH_FILE(client_fd, file, file_path,
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

        puts("file doesn't exist");
        printFs();

        // the new file has been successfully inserted
        START_WRITE(file, fs, 1, return -1);

        if( flags & O_LOCK ){
            // O_LOCK has been specified
            // let's make the new file locked by client_fd
            ec( clientListTailInsertion(&(file->locked_by), client_fd), -1, return -1 );

            // this client is the one who can write on this new file
            file->writeOwner = client_fd;
        }

        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if( flags & O_CREATE ){
            // O_CREATE has been specified
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 1;
        }

        puts("file exists");
        printFs();

        if(fs.eviction_policy == LRU)
            getFsFileToTail(file);

        // the new file exists
        START_WRITE(file, fs, 1, return -1);

        if( flags & O_LOCK ){
            // let's make it locked by client_fd
            ec( clientListTailInsertion(&(file->locked_by), client_fd), -1, return -1 );
        }
    );

    // let's make the new file opened by client_fd
    ec( clientListTailInsertion(&(file->opened_by), client_fd), -1, return -1 );

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
    LOCK_FS_SEARCH_FILE(client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, WRONG_FILEPATH_RESPONSE ), -1, return -1 );
        return 1;
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        START_WRITE( file, fs, 1, return -1 );

        // update the writeOwner
        file->writeOwner = -1;

        // does client_fd previously opened the file?
        if( removeClientFromList(client_fd, file->opened_by) == 0 ){
            // a client was removed
            ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );
        }else{
            // client not in list
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
        }

        DONE_WRITE(file, fs, , return -1);
    );
    return 0;
}

/**
 * Read a file
 * @param msgPtr: the message we shall send back to the client
 * @param client_fd: the client
 * @param file_path: the path of the file to read
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. reading an unopened file)
 *      0 otherwise
 */ 
int readFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    file_t* file;
    LOCK_FS_SEARCH_FILE(client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, WRONG_FILEPATH_RESPONSE ), -1, return -1 );
        return 1;
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if(fs.eviction_policy == LRU)
            getFsFileToTail(file);

        START_READ( file, fs, 1, return -1 );

        // is the file locked by someone else? OR is the client opened by client_fd?
        if( (file->locked_by != NULL && (file->locked_by)->client_fd != client_fd) || 
                searchClientInList(client_fd, file->opened_by)!=0 ){
            // file is locked by someone else
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 1;
        }
        // the client previously opened the file and is, maybe, the locker
        
        if(file->content == NULL){
            ec( buildMsg(msgPtr, NO_CONTENT_FILE ), -1, return -1 );
            return 1;
        }

        sendContentToClient(client_fd, file->content, file->size, READ_FILE_CONTENT);
        ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );

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
    return 1;
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
    LOCK_FS_SEARCH_FILE(client_fd, file, file_path,
        // - NOT FOUND PROCEDURE --------------------------------------------------------------------
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
        ec( buildMsg(msgPtr, WRONG_FILEPATH_RESPONSE ), -1, return -1 );
        return 1;
        , // - FOUND PROCEDURE ----------------------------------------------------------------------
        if(client_fd != file->writeOwner){
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
        
        // compare new storage size and capacity AND new current files number and capacity
        int newFsSize = contentSize + fs.currSize;
        if(newFsSize > fs.byte_capacity || fs.currFilesNumber +1 > fs.file_capacity){
            // eviction needed
            ec( evictFiles(newFsSize - fs.byte_capacity, fs.currFilesNumber +1 - fs.file_capacity,
                client_fd), -1, return -1 );
        }

        // don't unlock the store here because of readNfiles handling: 
        // deadlock if some N files reader locks the store
        START_WRITE( file, fs, 0, return -1 );
        // NOTE: UNLOCK(fs.mux) NEEDED

        ec( appendContentToFile(file, content, contentSize), -1, return -1 );

        ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );

        DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
    );
    return 0;
    return 1;
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
    LOCK_FS_SEARCH_FILE(client_fd, file, file_path,
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
        
        // compare new storage size and capacity
        if(newFsSize > fs.byte_capacity){
            // eviction needed
            ec( evictFiles(newFsSize - fs.byte_capacity, -1, client_fd), -1, return -1 );
        }

        // don't unlock the store here because of readNfiles handling: 
        // deadlock if some N files reader locks the store
        START_WRITE( file, fs, 0, return -1 );
        // NOTE: UNLOCK(fs.mux) NEEDED

        // is the file locked by someone else? OR is the client opened by client_fd?
        if( (file->locked_by != NULL && (file->locked_by)->client_fd != client_fd) || 
                searchClientInList(client_fd, file->opened_by)!=0 ){
            // file is locked by someone else
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            return 1;
        }
        // the client previously opened the file and is, maybe, the locker
        
        ec( appendContentToFile(file, content, contentSize), -1, return -1 );

        ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );

        DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
        ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
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
    LOCK_FS_SEARCH_FILE(client_fd, file, file_path,
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
            ec( clientListTailInsertion(&(file->locked_by), client_fd), -1, return -1 );
            ec( buildMsg(msgPtr, NULL ), -1, return -1 );
            return 0;
        }else{
            // file's locked by no one or locked by the same client who did the request
            // NOTE: specific says the operation ends successfully even if the client already
            // holds the lock
            if( file->locked_by == NULL )
                ec( clientListTailInsertion(&(file->locked_by), client_fd), -1, return -1 );
        }
        ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );
        DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
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
    LOCK_FS_SEARCH_FILE(client_fd, file, file_path,
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
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            return 1;
        }else{
            // file's locked by the same client who did the request
            ec( buildMsg(msgPtr, FINE_REQ_RESPONSE ), -1, return -1 );
        }
        DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
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
    LOCK_FS_SEARCH_FILE(client_fd, file, file_path,
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
            ec( buildMsg(msgPtr, NOT_PERMITTED_ACTION_RESPONSE ), -1, return -1 );
            DONE_WRITE(file, fs, file->writeOwner = -1;, return -1);
            ec( SAFE_UNLOCK(fs.mux), 1, return -1; );
            return 1;
        }else{
            // file's locked by the same client who did the request
            deleteFile(file, -1, 0);
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
    puts("destroying fs, FILES:");
    file_t* nextNode;
    while(list != NULL){
        nextNode = list->next;
        printf("%s\n %s\n\n", list->path, list->content);
        free( destroyFile(list, 0) ); // it returns the content of the file. We free it too
        list = nextNode;
    }
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
 * @param sendContentBackToClient: says if we want to send the content back to clientFd
 * @return
 *      -1 if fatal error
 *      0 otherwise
 */
int deleteFile(file_t* filePtr, client_fd_t clientFd, char sendContentBackToClient){
    if( filePtr == NULL)
        return 0;
    char* tempContent = NULL;
    
    // detach file from the storage
    detachFileFromStorage(filePtr);

    icl_hash_delete(fs.hFileTable, filePtr->path, NULL, NULL);
    fs.currFilesNumber--;
    fs.currSize -= filePtr->size;
    int contentSize = filePtr->size;
    tempContent = destroyFile(filePtr, 1);
    if( sendContentBackToClient ){
        if( sendContentToClient(clientFd, tempContent, contentSize, REMOVED_FILE_CONTENT) == -1 ){
            free(tempContent);
            return -1;
        }
    }
    free(tempContent);
    return 0;
}