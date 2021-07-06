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
    ec( (*filePtr)->path = calloc(strlen(path) + 1, sizeof(char)), NULL, return -1 );
    (*filePtr)->content = NULL;
    (*filePtr)->size = 0;
    (*filePtr)->locked_by = NULL;
    (*filePtr)->opened_by = NULL;
    (*filePtr)->writeOwner = -1;
    (*filePtr)->readerCount = 0;
    (*filePtr)->writerCount = 0;
    pthread_cond_init(&((*filePtr)->cv), NULL);
    pthread_mutex_init(&((*filePtr)->mux), NULL);
    pthread_mutex_init(&((*filePtr)->order), NULL);
    (*filePtr)->prev = NULL;
    (*filePtr)->next = NULL;
    return 1;
}

/**
 * Free the whole list of client_t list
 * @param list: the list to free
 * @param file: the file related to the list
 * @param warnClients: flag: warn the clients about the distruction of the file
 */
void freeClientsList(client_t* list, char* filePath, char warnClients){
    client_t* nextNode;
    while(list != NULL){
        nextNode = list->next;
        if(warnClients && list->client_fd > 0)
            // Send a message to the client clientFd, warning him the file was removed
            sendTo(list->client_fd, filePath, strlen(filePath));
        free(list);
        list = nextNode;
    }
}

/**
 * Destroy a file freeing the memory and destroying mutexes and condition variable
 * @param file: the pointer to the file
 * @return the file content
 */
char* destroyFile(file_t* file, char warn_lockers){
    freeClientsList(file->locked_by, file->path, warn_lockers);
    freeClientsList(file->opened_by, NULL, 0);
    free(file->path);
    pthread_mutex_destroy(&(file->mux));
    pthread_mutex_destroy(&(file->order));
    pthread_cond_destroy(&(file->cv));
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
 *      1 if everything's ok
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
    return 1;
}

/*----------- Middleware -----------------------------------------------------------------------*/
/**
 * Put the client into the openers list of the file referred by file_path.
 * The file is created if it doesn't exist and flags==O_CREATE.
 * The procedure fails with logical error if the file exists and (flags & O_CREATE == 1)
 * or if the file doesn't exist and (flags & O_CREATE == 0)
 * 
 * @return 
 *      -1 if fatal error occurred
 *      1 if logical error occurred (i.e. creation of an existing file...)
 *      0 otherwise
 */ 
int openFile(msg_t* msgPtr, client_fd_t client_fd, char flags, char* file_path){
    ec( SAFE_LOCK(fs.mux), 1, return -1 );
    file_t* file = searchFile(file_path);
    if(file==NULL){
        // the file is not in the storage
        if( !(flags & O_CREATE) ){
            // O_CREATE is not part of the flags
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            return 1;
        }
        
        ec( file = malloc(sizeof(file_t)), NULL, return -1 );

        // O_CREATE has been specified
        if( buildNewFile(&file, file_path) == -1 ){
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            return -1;
        }
        
        // insert file performing eviction if needed (maybe we get over the file_capacity limit)
        int fileInsertion = insertFile(file, PERFORM_EVICTION, client_fd);
        if( fileInsertion == -1 ){
            // fatal error
            free( destroyFile(file, 0));
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            return -1;
        }

        // the new file has been successfully inserted
        ec( SAFE_LOCK(file->mux), 1, return -1 );

        if( flags & O_LOCK ){
            // O_LOCK has been specified
            // let's make the new file locked by client_fd
            clientListTailInsertion(&(file->locked_by), client_fd);

            // this client is the one who can write on this new file
            file->writeOwner = client_fd;
        }
    }else{
        if( flags & O_CREATE ){
            // O_CREATE has been specified
            ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
            return 1;
        }

        if( flags & O_LOCK ){
            // let's make it locked by client_fd
            clientListTailInsertion(&(file->locked_by), client_fd);
        }
    }

    // let's make the new file opened by client_fd
    clientListTailInsertion(&(file->opened_by), client_fd);

    ec( SAFE_UNLOCK(fs.mux), 1, return -1 );
    // build response
    ec( msgPtr->content = strdup(FINE_REQ_RESPONSE), NULL, return -1 );
    msgPtr->len = strlen(FINE_REQ_RESPONSE); 
    return 0;
}

int closeFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    return 1;
}

int readFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    return 1;
}

int readNFiles(msg_t* msgPtr, client_fd_t client_fd, int N){
    return 1;
}

int writeFile(msg_t* msgPtr, client_fd_t client_fd, char* content, char* file_path){
    return 1;
}

int appendToFile(msg_t* msgPtr, client_fd_t client_fd, char* content, char* file_path){
    return 1;
}

int lockFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    return 1;
}

int unlockFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    return 1;
}

int removeFile(msg_t* msgPtr, client_fd_t client_fd, char* file_path){
    return 1;
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
    file_t* nextNode;
    while(list != NULL){
        nextNode = list->next;
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

    if(fs.currFilesNumber == 0 /* && (fs.currSize==0) */){
        /* storage is empty */
        /* allowed to insert the file here */
        fs.fileListHead = filePtr;
        fs.fileListTail = fs.fileListHead;
    }else{
        /* storage is not empty */
        if( eviction_needed ){
            if(performEviction){
                file_t** evictionVictims = NULL; // list of pointers to the victims
                unsigned int evVictimsArrayLength = 0;
                evictionVictims = getEvictionVictimsList(newSize - fs.byte_capacity, 
                                                            newFilesNum - fs.file_capacity,
                                                            &evVictimsArrayLength);
                if( evictFiles(evictionVictims, evVictimsArrayLength, client_fd) == -1 ){
                    // fatal error occurred while evicting files
                    // TODO: free stuff
                    return -1;
                }
                free(evictionVictims);
            }else{
                //got a serious problem here
                return -1;
            }
        }
        filePtr->prev = fs.fileListTail;
        fs.fileListTail->next = filePtr;
    }

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
 * Push the file x in the evictionVictims array
 * @param evictionVictims: the address of the file_t pointers array
 * @param length: the pointer to the length variable (length of the array)
 * @param p: the pointer we want to push
 * @return 
 *      -1 if fatal error occurred
 *      0 if everything's ok
 */
int pushFilePointerInArray(file_t*** evictionVictims, unsigned int* length, file_t* p){
    if( checkMultiplicationOverflow(((*length)+1), sizeof(file_t*)) == -1 )
        return -1;
    int newSize = ((*length)+1) * sizeof(file_t*);
    ec( *evictionVictims = realloc( *evictionVictims, newSize ), NULL, return -1 );
    (*evictionVictims)[*length] = p;
    (*length)++;
    return 0;
}

/**
 * Build a list of files which are the next victims of the eviction policy
 * @param bytesNum: the number of bytes we need to free
 * @param filesNum: the number of files we need to evict
 * @return 
 *      the list of the pointers to the victims
 */
file_t** getEvictionVictimsList(int bytesNum, int filesNum, unsigned int * length){
    file_t** evictionVictims = NULL;
    *length = 0;
    file_t* currFile = fs.fileListHead;
    while(currFile != NULL){
        if( filesNum > 0 || bytesNum > 0){
            // we need to evict someone
            if( fs.eviction_policy == FIFO || fs.eviction_policy == LRU ){
                // we evict the less recently inserted files (from fs.fileListHead ahead)
                pushFilePointerInArray(&evictionVictims, length, currFile);
            }
        }
        currFile = currFile->next;
    }
    return evictionVictims;
}

/**
 * Send content to the client clientFd
 * @param clientFd: the client we want to send the message to
 * @param content: the string we are going to send
 * @return
 *      -1 if fatal error occurred
 *      0 if everything's ok
 */
int sendContentToClient(client_fd_t clientFd, char* content){
    if(content == NULL)
        return 0;
    char* str;
    int newLen = strlen(REMOVED_FILE_CONTENT)+ strlen(content);
    ec( str = calloc(newLen + 1, sizeof(char)), 
        NULL, return -1 
    );
    snprintf(str, sizeof(str)+1, "%s%s", REMOVED_FILE_CONTENT, content);
    sendTo(clientFd, str, newLen);
    return 0;
}

/**
 * Evict files
 * @param files: the list of pointers to the files in the storage
 * @return
 *      -1 if fatal error
 *      0 if everything's ok
 */
int evictFiles(file_t** files, unsigned int length, client_fd_t client_fd){
    if( files == NULL)
        return 0;
    char* tempContent = NULL;
    int i = 0;
    for(; i<length; ++i){
        if( files[i]->prev == NULL ){
            // this is the head of the list
            fs.fileListHead = files[i]->next;
            if( fs.fileListHead != NULL)
                (fs.fileListHead)->prev = NULL;
        }else{
            (files[i]->prev)->next = files[i]->next;
            (files[i]->next)->prev = files[i]->prev;
        }
        tempContent = destroyFile(files[i], 1);
        if( sendContentToClient(client_fd, tempContent) == -1 ){
            free(tempContent);
            return -1;
        }
        free(tempContent);
    }
    return 0;
}
