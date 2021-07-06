#if !defined(_SHARED_VALUES_UTIL_H)
#define _SHARED_VALUES_UTIL_H
#include <pthread.h>

/* USAGE:
//init synchronization struct for the termination shared value (quit flag)
INIT_SHARED_STRUCT(sharedStruct, char, 0, exit(EXIT_FAILURE);, 
    free(sharedStruct.value);
    exit(EXIT_FAILURE);
);

// read shared value
SHARED_VALUE_READ(sharedStruct,
    if( *((char*) sharedStruct.value ) != 0 )
        break; // do stuff
);

// write shared value
SHARED_VALUE_WRITE(sharedStruct,
    *((char*) sharedStruct.value) = 1;
    // do stuff
);

*/
#define INIT_SHARED_STRUCT(s, type, initVal, onMemError, onConcError)   \
    s.value = malloc(sizeof(type));                                    \
    if(s.value == NULL){                                                \
        onMemError                                                      \
    }else{                                                               \
        *((type*)s.value) = initVal;                                   \
        s.activeReaders = 0;                                           \
        s.activeWriters = 0;                                           \
        s.waitingReaders = 0;                                          \
        s.waitingWriters = 0;                                          \
        if( initSharedStructMuxNCondVar(&s) == -1){                      \
            onConcError                                                 \
        }                                                               \
    }                                                                   \
        
/* Read, unfair for readers */
#define SHARED_VALUE_READ(s, c)                 \
    startRead_not_readers_fair(&s);             \
    c                                           \
    doneRead_not_readers_fair(&s);              \

/* Write, unfair for readers */
#define SHARED_VALUE_WRITE(s, c)                \
    startWrite_not_readers_fair(&s);            \
    c                                           \
    doneWrite_not_readers_fair(&s);             \

// shared struct
typedef struct{
    void* value;
    pthread_mutex_t mutex;
    int activeWriters;
    int activeReaders;
    int waitingReaders;
    int waitingWriters;
    pthread_cond_t writeGo;
    pthread_cond_t readGo;
} SharedStruct;

int initSharedStructMuxNCondVar(SharedStruct*);
void destroySharedStruct(SharedStruct*);
void startRead_not_readers_fair(SharedStruct* );
void doneRead_not_readers_fair(SharedStruct* );
void startWrite_not_readers_fair(SharedStruct* );
void doneWrite_not_readers_fair(SharedStruct* );

void startRead_not_writers_fair(SharedStruct* );
void doneRead_not_writers_fair(SharedStruct* );
void startWrite_not_writers_fair(SharedStruct* );
void doneWrite_not_writers_fair(SharedStruct* );

void startRead_fair_fifo(SharedStruct *);
void doneRead_fair_fifo(SharedStruct *);
void startWrite_fair_fifo(SharedStruct *);
void doneWrite_fair_fifo(SharedStruct *);

#endif