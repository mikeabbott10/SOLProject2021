#if !defined(_FILESYSTEM_UTIL_H)
#define _FILESYSTEM_UTIL_H
#include <pthread.h>
#include <icl_hash.h>

#define FIFO 0x00
#define LRU 0x01

/*file abstraction*/
typedef struct{
    char* path; /*the path of the file*/
    char* content; /*the content of the file*/
    int size; /*the size/length of the file content*/

    void* locked_by; /*will be the list of clients who want to lock the file. 
                    The first one is the current locker, NULL if file's not locked*/
    void* opened_by; /*will be the list of clients who opened the file*/

    int readerCount;
    int writerCount;
    pthread_mutex_t mux;
    pthread_mutex_t ordering;
    pthread_cond_t go;

    file_t* prev;
    file_t* next;
} file_t;


typedef struct {
    file_t* fileListHead;
    file_t* fileListTail;
    icl_hash_t* hFileTable;

    size_t file_capacity; /* max number of files in the storage */
    size_t byte_capacity; /* max number of used bytes */
    size_t eviction_policy; /* FIFO or LRU */

    size_t currFilesNumber; /* current number of files in the storage */
    size_t currSizeNumber; /* current number of used bytes */
    
    /* for stats */
    size_t performed_evictions;
    size_t maxReachedSize;
    size_t maxReachedFilesNumber;
} FileSystem;

FileSystem fs;

int initFileSystem(size_t, size_t, char);
void destroyFileSystem();
int insertFile(file_t*, file_t***);
file_t** getEvictionVictimsList(int, int);



#endif