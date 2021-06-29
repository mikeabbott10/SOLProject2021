#include<filesystem_util.h>
#include<general_utility.h>
#include<stdlib.h>

/**
 * Initialize the filesystem structure
 * @param file_capacity: max number of stored files
 * @param byte_capacity: max number of bytes used by the storage
 * @param eviction_policy: the eviction policy of the filesystem
 * @return
 *      1 if the creation of the hash table fails or any value is not valid
 *      0 otherwise
 */
int initFileSystem(size_t file_capacity, size_t byte_capacity, char eviction_policy){
    fs.fileListHead = NULL;
    fs.fileListTail = NULL;
    /* init hash table */
    ec( fs.hFileTable = icl_hash_create(file_capacity,NULL,NULL), 
        NULL, return 1
    );
    /* check values */
    if(file_capacity<=0 || byte_capacity<=0 || (eviction_policy!=FIFO && eviction_policy!=LRU)){
        /* destroy hash table */
        icl_hash_destroy(fs.hFileTable,free,NULL);
        return 1;
    }
    fs.file_capacity = file_capacity;
    fs.byte_capacity = byte_capacity;
    fs.eviction_policy = eviction_policy;
    fs.currFilesNumber = 0;
    fs.currSizeNumber = 0;
    fs.maxReachedFilesNumber = 0;
    fs.maxReachedSize = 0;
    fs.performed_evictions = 0;
    return 0;
}

/**
 * Destroy the filesystem freeing the memory
 */
void destroyFileSystem(){
    freeList(fs.fileListHead);
    /* destroy hash table */
    icl_hash_destroy(fs.hFileTable,free,NULL);
}

/**
 * Insert a file into the file list. A tail insertion is performed.
 * @param file: the file
 * @param evictionVictimFilesPtr: address of the list of the pointers to the next victims
 * @return 
 *      2 got a fatal error
 *      1 if insertion is not permitted (file too large)
 *      0 otherwise
 */
int insertFile(file_t* filePtr, file_t*** evictionVictimFilesPtr){
    char eviction_needed = 0;
    /* check new sizes */
    int newSize = filePtr->size + fs.currSizeNumber;
    int newFilesNum = fs.currFilesNumber + 1;
    if(newSize > fs.byte_capacity || newFilesNum > fs.file_capacity)
        eviction_needed = 1;

    if(fs.currFilesNumber == 0){
        /* storage is empty */
        if(eviction_needed)
            /* can't insert this file. Too large */
            return 1;
        /* allowed to insert the file here */
        fs.fileListHead = filePtr;
        fs.fileListTail = fs.fileListHead;
    }else{
        /* storage is not empty */
        if(eviction_needed){
            *evictionVictimFilesPtr = getEvictionVictimsList(newSize - fs.byte_capacity, 
                                                        newFilesNum - fs.file_capacity);
        }
        filePtr->prev = fs.fileListTail;
        fs.fileListTail->next = filePtr;
    }

    /* we inserted the file here, let's put it in the hash table and update the size values */
    /* update size */
    fs.currSizeNumber += filePtr->size;
    SET_MAX(fs.maxReachedSize, fs.currSizeNumber);
    
    /* update current files number */
    fs.currFilesNumber++;
    SET_MAX(fs.maxReachedFilesNumber, fs.currFilesNumber);
    
    ec( icl_hash_insert(fs.hFileTable, filePtr->path, filePtr), NULL, return 2);

    return 0;
}

/**
 * Build a list of files which are the next victims of the eviction policy
 * @param bytesNum: the number of bytes we need to free
 * @param filesNum: the number of files we need to evict
 * @return the list of the pointers to the victims
 */
file_t** getEvictionVictimsList(int bytesNum, int filesNum){
    //TODO
}
