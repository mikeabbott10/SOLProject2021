#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include<shared_values_util.h>

/**
 * Initialize the shared struct lock and condition variables
 * @return 
 *      -1 if fatal error occurred
 *      0 otherwise
 */
int initSharedStructMuxNCondVar(SharedStruct* s){
    if( pthread_mutex_init(&(s->mutex), NULL) != 0 ||
            pthread_mutex_init(&(s->order), NULL) != 0 ||
            pthread_cond_init(&(s->writeGo), NULL) != 0 ||
            pthread_cond_init(&(s->readGo), NULL) != 0 )
        return -1;
    return 0;
}

/**
 * Free the memory and destroy lock and conditions variables
 */
void destroySharedStruct(SharedStruct* s){
    free(s->value);
    pthread_cond_destroy(&(s->writeGo));
    pthread_cond_destroy(&(s->readGo));
    pthread_mutex_destroy(&(s->mutex));
    pthread_mutex_destroy(&(s->order));
}

/*----------- Not fair for readers solution ---------------------------------------------------------*/
#pragma region

int startRead_not_readers_fair(SharedStruct* s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    //printf("\t Thread: %ld in coda\n", syscall(__NR_gettid));
    s->waitingReaders++;
    while(s->activeWriters > 0 || s->waitingWriters > 0){
        pthread_cond_wait(&(s->readGo), &(s->mutex));
    }
    s->waitingReaders++;
    s->activeReaders++;
    //printf("\t Thread: %ld uscito dalla coda\n", syscall(__NR_gettid));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}

int doneRead_not_readers_fair(SharedStruct* s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->activeReaders--;
    if(s->activeReaders==0 && s->waitingWriters > 0)
        pthread_cond_signal(&(s->writeGo));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}

int startWrite_not_readers_fair(SharedStruct* s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->waitingWriters++;
    //printf("\t Thread: %ld in coda\n", syscall(__NR_gettid));
    if(s->activeReaders > 0 || s->activeWriters > 0){
        pthread_cond_wait(&(s->writeGo), &(s->mutex));
    }
    s->waitingWriters--;
    s->activeWriters++;
    //printf("\t Thread: %ld uscito dalla coda\n", syscall(__NR_gettid));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}

int doneWrite_not_readers_fair(SharedStruct* s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->activeWriters--;
    if(s->waitingWriters > 0)
        pthread_cond_signal(&(s->writeGo));
    else
        pthread_cond_broadcast(&(s->readGo));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}
#pragma endregion

/*----------- Not fair for writers solution ---------------------------------------------------------*/
#pragma region

int startRead_not_writers_fair(SharedStruct* s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    //printf("\t Thread: %ld in coda\n", syscall(__NR_gettid));
    s->waitingReaders++;
    while(s->activeWriters > 0){
        pthread_cond_wait(&(s->readGo), &(s->mutex));
    }
    s->waitingReaders++;
    s->activeReaders++;
    //printf("\t Thread: %ld uscito dalla coda\n", syscall(__NR_gettid));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 );
    return 0;
}

int doneRead_not_writers_fair(SharedStruct* s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->activeReaders--;
    if(s->activeReaders==0 && s->waitingWriters > 0)
        pthread_cond_signal(&(s->writeGo));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}

int startWrite_not_writers_fair(SharedStruct* s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->waitingWriters++;
    //printf("\t Thread: %ld in coda\n", syscall(__NR_gettid));
    if(s->activeReaders > 0 || s->activeWriters > 0){
        pthread_cond_wait(&(s->writeGo), &(s->mutex));
    }
    s->waitingWriters--;
    s->activeWriters++;
    //printf("\t Thread: %ld uscito dalla coda\n", syscall(__NR_gettid));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}

int doneWrite_not_writers_fair(SharedStruct* s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->activeWriters--;
    if(s->waitingReaders > 0)
        pthread_cond_broadcast(&(s->readGo));
    else
        pthread_cond_signal(&(s->writeGo));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}
#pragma endregion

/*----------- Fair solution, non FIFO policy --------------------------------------------------------*/
#pragma region

// we only need one condition variable here. Let's use writeGo

int startRead_fair(SharedStruct *s){
    ec( safe_mutex_lock(&(s->order)), 1, return -1 );
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    while(s->activeWriters > 0)
        pthread_cond_wait(&(s->writeGo), &(s->mutex));
    s->activeReaders++;
    ec( safe_mutex_unlock(&(s->order)), 1, return -1 );
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 );
    return 0;
}

int doneRead_fair(SharedStruct *s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->activeReaders--;
    if( s->activeReaders == 0 )
        pthread_cond_signal(&(s->writeGo));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 );
    return 0;
}

int startWrite_fair(SharedStruct *s){
    ec( safe_mutex_lock(&(s->order)), 1, return -1 );
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    while(s->activeWriters > 0 || s->activeReaders > 0)
        pthread_cond_wait(&(s->writeGo), &(s->mutex));
    s->activeWriters++;
    ec( safe_mutex_unlock(&(s->order)), 1, return -1 );
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 );
    return 0;
}

int doneWrite_fair(SharedStruct *s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->activeWriters--;
    pthread_cond_signal(&(s->writeGo));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 );
    return 0;
}

#pragma endregion

/*----------- Fair solution, FIFO access policy -----------------------------------------------------*/
#pragma region

// --------- FIFO UTIL
typedef struct node{
    void* item;
    struct node* prev;
    struct node* next;
} Node;

int appendTail(Node** lPtr, void* item){
    Node *newNode = (Node*) malloc(sizeof(Node));
    if(newNode==NULL)
        return -1;
    newNode->next = NULL;
    newNode->item = item;
    if(*lPtr != NULL){
        Node *currNode = *lPtr;
        while(currNode != NULL){
            if(currNode->next != NULL)
                currNode = currNode->next;
            else break;
        }
        currNode->next = newNode;
    }else
        *lPtr = newNode;
    return 0;
}

void *popHead(Node **lPtr){
    if(*lPtr == NULL)
        return NULL;
    Node *nodo = *lPtr;
    *lPtr = (*lPtr)->next;
    void* item = malloc(sizeof(void*));
    if(item == NULL) return NULL;
    item = nodo->next;
    //memcpy(item, nodo->item, sizeof(nodo->item));
    free(nodo);
    return item;
}

char is_empty(Node *lPtr){
    return lPtr == NULL;
}
// --------- END FIFO UTIL

Node* waiting = NULL;

int startRead_fair_fifo(SharedStruct *s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    //printf("\t Thread: %ld in coda\n", syscall(__NR_gettid));
    if(s->activeWriters > 0){
        pthread_cond_t self;
        pthread_cond_init(&self, NULL);
        appendTail(&waiting, &self);
        do{
            pthread_cond_wait(&self, &(s->mutex));
        }while(s->activeWriters > 0);
        free(popHead(&waiting));
        pthread_cond_destroy(&self);
    }
    s->activeReaders++;
    //printf("\t Thread: %ld uscito dalla coda\n", syscall(__NR_gettid));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}

int doneRead_fair_fifo(SharedStruct *s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->activeReaders--;
    if(s->activeReaders==0)
        if(!is_empty(waiting))
            pthread_cond_signal((pthread_cond_t *)(waiting->item)); // signal to the first one
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}

int startWrite_fair_fifo(SharedStruct *s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    //printf("\t Thread: %ld in coda\n", syscall(__NR_gettid));
    if(s->activeReaders > 0 || s->activeWriters > 0){
        pthread_cond_t self;
        pthread_cond_init(&self, NULL);
        appendTail(&waiting, &self);
        do{
            pthread_cond_wait(&self, &(s->mutex));
        }while(s->activeReaders > 0 || s->activeWriters > 0);
        free(popHead(&waiting));
        pthread_cond_destroy(&self);
    }
    s->activeWriters++;
    //printf("\t Thread: %ld uscito dalla coda\n", syscall(__NR_gettid));
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}

int doneWrite_fair_fifo(SharedStruct *s){
    ec( safe_mutex_lock(&(s->mutex)), 1, return -1 );
    s->activeWriters--;
    if(!is_empty(waiting))
        pthread_cond_signal((pthread_cond_t *)(waiting->item)); // signal to the first one
    ec( safe_mutex_unlock(&(s->mutex)), 1, return -1 ); // mutex.Release()
    return 0;
}

#pragma endregion