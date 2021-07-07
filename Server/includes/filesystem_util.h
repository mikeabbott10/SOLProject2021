#if !defined(_FILESYSTEM_UTIL_H)
#define _FILESYSTEM_UTIL_H
#define _POSIX_C_SOURCE 200809L
#include<errno.h>
#include <pthread.h>
#include <icl_hash.h>
#include<general_utility.h>

#define FIFO 0x00 /*if modified, please edit the config file range for eviction_policy*/
#define LRU 0x01

#define SAFE_LOCK(m) safe_mutex_lock(&m)
#define SAFE_UNLOCK(m) safe_mutex_unlock(&m)
#define CV_WAIT(c_v, m) pthread_cond_wait(&c_v, &m)
#define CV_SIGNAL(c_v) pthread_cond_signal(&c_v)

#define NO_EVICTION 0
#define PERFORM_EVICTION 1

#define START_WRITE(filePtr, fs, unlockStore, errorProc)            \
    ec( SAFE_LOCK(filePtr->order), 1, errorProc );                  \
    ec( SAFE_LOCK(filePtr->mux), 1, errorProc );                    \
    if(unlockStore) ec( SAFE_UNLOCK(fs.mux), 1, errorProc );        \
    while(filePtr->writersCount > 0 || filePtr->readersCount > 0)   \
        CV_WAIT(filePtr->Go, filePtr->mux);                         \
    filePtr->writersCount++;                                        \
    ec( SAFE_UNLOCK(filePtr->order), 1, errorProc );                \
    ec( SAFE_UNLOCK(filePtr->mux), 1, errorProc );                  \

#define DONE_WRITE(filePtr, fs, c, errorProc)                       \
    ec( SAFE_LOCK(filePtr->mux), 1, errorProc );                    \
    c                                                               \
    filePtr->writersCount--;                                        \
    ec_n( CV_SIGNAL(filePtr->Go), 0, errorProc );                   \
    ec( SAFE_UNLOCK(filePtr->mux), 1, errorProc );                  \

#define START_READ(filePtr, fs, unlockStore, errorProc)             \
    ec( SAFE_LOCK(filePtr->order), 1, errorProc );                  \
    ec( SAFE_LOCK(filePtr->mux), 1, errorProc );                    \
    if(unlockStore) ec( SAFE_UNLOCK(fs.mux), 1, errorProc );        \
    while(filePtr->writersCount > 0)                                \
        CV_WAIT(filePtr->Go, filePtr->mux);                         \
    filePtr->readersCount++;                                        \
    ec( SAFE_UNLOCK(filePtr->order), 1, errorProc );                \
    ec( SAFE_UNLOCK(filePtr->mux), 1, errorProc );                  \

#define DONE_READ(filePtr, fs, c, errorProc)                        \
    ec( SAFE_LOCK(filePtr->mux), 1, errorProc );                    \
    c                                                               \
    filePtr->readersCount--;                                        \
    if( filePtr->readersCount == 0 )                                \
        ec_n( CV_SIGNAL(filePtr->Go), 0, errorProc );               \
    ec( SAFE_UNLOCK(filePtr->mux), 1, errorProc );                  \

#define LOCK_FS_SEARCH_FILE(clientFd, filePtr, path, not_found_p, found_p)  \
    ec( SAFE_LOCK(fs.mux), 1, return -1 );                                  \
    printf("fd locked the storage: %d\n", clientFd);                        \
    filePtr = searchFile(path);                                             \
    if(filePtr == NULL){ not_found_p }                                      \
    else{ found_p }                                                         \

/*file abstraction*/
typedef struct file{
    char* path; /*the path of the file*/
    char* content; /*the content of the file*/
    int size; /*the size/length of the file content*/

    client_t* locked_by; /* the list of clients who want to lock the file. 
                    The first one is the current locker, NULL if file's not locked*/
    client_t* opened_by; /* the list of clients who opened the file*/
    client_fd_t writeOwner; /* the client who has just called openFile(path, O_LOCK|O_CREATE) */

    int readersCount;
    int writersCount;
    pthread_mutex_t mux;
    pthread_mutex_t order;
    pthread_cond_t Go;

    struct file* prev;
    struct file* next;
} file_t;


typedef struct {
    file_t* fileListHead;
    file_t* fileListTail;
    icl_hash_t* hFileTable;

    size_t file_capacity; /* max number of files in the storage */
    size_t byte_capacity; /* max number of used bytes */
    size_t eviction_policy; /* FIFO or LRU */

    size_t currFilesNumber; /* current number of files in the storage */
    size_t currSize; /* current number of used bytes */
    
    /* for stats */
    size_t performed_evictions;
    size_t maxReachedSize;
    size_t maxReachedFilesNumber;

    pthread_mutex_t mux;
} FileSystem;

FileSystem fs;

/*----------- Middleware -----------------------------------------------------------------------*/
int openFile(msg_t*, client_fd_t, char, char*);
int closeFile(msg_t*, client_fd_t, char*);
int readFile(msg_t*, client_fd_t, char*);
int readNFiles(msg_t*, client_fd_t, int);
int writeFile(msg_t*, client_fd_t, char*, char*);
int appendToFile(msg_t*, client_fd_t, char*, char*);
int lockFile(msg_t*, client_fd_t, char*);
int unlockFile(msg_t*, client_fd_t, char*);
int removeFile(msg_t*, client_fd_t, char*); 

/*----------- Backend --------------------------------------------------------------------------*/
int initFileSystem(size_t, size_t, char);
void destroyFileSystem();
int insertFile(file_t*, char, client_fd_t);
int evictFiles(int, int, client_fd_t);
int deleteFile(file_t*, client_fd_t, char);

#endif