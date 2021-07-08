#if !defined(_SHARED_VALUES_UTIL_H)
#define _SHARED_VALUES_UTIL_H
#include <pthread.h>
#include<general_utility.h>

/* USAGE:
//init synchronization struct for the shared value
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
    s.value = malloc(sizeof(type));                                     \
    if(s.value == NULL){                                                \
        onMemError                                                      \
    }else{                                                              \
        *((type*)s.value) = initVal;                                    \
        s.activeReaders = 0;                                            \
        s.activeWriters = 0;                                            \
        s.waitingReaders = 0;                                           \
        s.waitingWriters = 0;                                           \
        if( initSharedStructMuxNCondVar(&s) == -1){                     \
            onConcError                                                 \
        }                                                               \
    }                                                                   \
        
/* Read, fair non fifo policy */
#define SHARED_VALUE_READ(s, c, errorProc)      \
    ec( startRead_fair(&s), -1, errorProc );    \
    c                                           \
    ec( doneRead_fair(&s), -1, errorProc );     \

/* Write, fair non fifo policy */
#define SHARED_VALUE_WRITE(s, c, errorProc)     \
    ec( startWrite_fair(&s), -1, errorProc );    \
    c                                           \
    ec( doneWrite_fair(&s), -1, errorProc );     \

// shared struct
typedef struct{
    void* value;
    pthread_mutex_t mutex;
    pthread_mutex_t order;
    unsigned int activeWriters;
    unsigned int activeReaders;
    unsigned int waitingReaders;
    unsigned int waitingWriters;
    pthread_cond_t writeGo;
    pthread_cond_t readGo;
} SharedStruct;

int initSharedStructMuxNCondVar(SharedStruct*);
void destroySharedStruct(SharedStruct*);

int startRead_not_readers_fair(SharedStruct* );
int doneRead_not_readers_fair(SharedStruct* );
int startWrite_not_readers_fair(SharedStruct* );
int doneWrite_not_readers_fair(SharedStruct* );

int startRead_not_writers_fair(SharedStruct* );
int doneRead_not_writers_fair(SharedStruct* );
int startWrite_not_writers_fair(SharedStruct* );
int doneWrite_not_writers_fair(SharedStruct* );

int startRead_fair(SharedStruct *);
int doneRead_fair(SharedStruct *);
int startWrite_fair(SharedStruct *);
int doneWrite_fair(SharedStruct *);

int startRead_fair_fifo(SharedStruct *);
int doneRead_fair_fifo(SharedStruct *);
int startWrite_fair_fifo(SharedStruct *);
int doneWrite_fair_fifo(SharedStruct *);

#endif