#if !defined(_GENERIC_BUF_H)
#define _GENERIC_BUF_H
#include <general_utility.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

// -------------------------------------------------
//  SIMPLE GENERIC SHARED RING BUFFER, FIFO POLICY.
//               author: gio54321
//       adjusted for the project by: lorenzo
// -------------------------------------------------

// EXAMPLE USAGE:
/*
declare_shared_buffer_use(int, int_buffer);
int main(void)
{
    int_buffer* b = shared_buffer_create(int_buffer, int, 10);

    for (int i = 0; i < 10; i++) {
        shared_buffer_put(int_buffer, int, b, i);
        printf("put %d\n", i);
    }
    for (int i = 0; i < 10; i++) {
        printf("got %d\n", shared_buffer_get(int_buffer, int, b));
    }

    shared_buffer_free(int_buffer, int, b);
} 
*/

/**
 * Safe mutex lock
 * @param mux: the mutex to lock
 */
static inline void safe_mutex_lock(pthread_mutex_t* mux){
    int err;
    if ((err = pthread_mutex_lock(mux)) != 0) {
        errno = err;
        perror("lock");
        pthread_exit(NULL);
    }/* else {
        //printf("locked\n");
    }*/
}

/**
 * Unsafe mutex lock
 * @param mux: the mutex to unlock
 */
static inline void safe_mutex_unlock(pthread_mutex_t* mux){
    int err;
    if ((err = pthread_mutex_unlock(mux)) != 0) {
        errno = err;
        perror("unlock");
        pthread_exit(NULL);
    }/* else {
        //printf("unlocked\n");
    }*/
}

#define declare_shared_buffer_use(T, BT)                               \
    typedef struct {                                                   \
        T* buffer;                                                     \
        size_t capacity;                                               \
        size_t start;                                                  \
        size_t end;                                                    \
        pthread_mutex_t mux;                                           \
        pthread_cond_t not_anymore_empty;                              \
        pthread_cond_t not_anymore_full;                               \
    } BT;                                                              \
                                                                       \
    BT* shared_buffer_create_##BT(size_t capacity)                     \
    {                                                                  \
        BT* b;                                                         \
        ec( b = (BT*)malloc(sizeof(BT)), NULL, return NULL );          \
        b->capacity = capacity + 1;                                    \
        b->start = 0;                                                  \
        b->end = 0;                                                    \
        b->buffer = (T*)malloc((capacity + 1) * sizeof(T));            \
        if(b->buffer == NULL){                                         \
            free(b);                                                   \
            return NULL;                                               \
        }else{                                                         \
            if( pthread_mutex_init(&(b->mux), NULL) != 0 ||            \
            pthread_cond_init(&(b->not_anymore_empty), NULL) != 0 ||   \
            pthread_cond_init(&(b->not_anymore_full), NULL) != 0 ){    \
                free(b->buffer);                                       \
                free(b);                                               \
                return NULL;                                           \
            }                                                          \
        }                                                              \
        return b;                                                      \
    }                                                                  \
                                                                       \
    void shared_buffer_free_##BT(BT* b)                                \
    {                                                                  \
        free(b->buffer);                                               \
        pthread_mutex_destroy(&(b->mux));                              \
        pthread_cond_destroy(&(b->not_anymore_empty));                 \
        pthread_cond_destroy(&(b->not_anymore_full));                  \
        free(b);                                                       \
    }                                                                  \
                                                                       \
    void shared_buffer_put_##BT(BT* b, T x)                            \
    {                                                                  \
        safe_mutex_lock(&(b->mux));                                    \
        while (b->start == (b->end + b->capacity + 1) % b->capacity && \
                !globalQuit) {                                         \
            pthread_cond_wait(&(b->not_anymore_full), &(b->mux));      \
        }                                                              \
        if(globalQuit) {                                               \
            safe_mutex_unlock(&(b->mux));                              \
            return;                                                    \
        }                                                              \
        b->buffer[b->end] = x;                                         \
        ++(b->end);                                                    \
        b->end %= b->capacity;                                         \
        pthread_cond_signal(&(b->not_anymore_empty));                  \
        safe_mutex_unlock(&(b->mux));                                  \
    }                                                                  \
                                                                       \
    T shared_buffer_get_##BT(BT* b, T nullValue)                       \
    {                                                                  \
        T result;                                                      \
        safe_mutex_lock(&(b->mux));                                    \
        while (b->start == b->end && !globalQuit) {                    \
            pthread_cond_wait(&(b->not_anymore_empty), &(b->mux));     \
        }                                                              \
        if(globalQuit) {                                               \
            safe_mutex_unlock(&(b->mux));                              \
            return nullValue;                                          \
        }                                                              \
        result = b->buffer[b->start];                                  \
        ++(b->start);                                                  \
        b->start %= b->capacity;                                       \
        pthread_cond_signal(&(b->not_anymore_full));                   \
        safe_mutex_unlock(&(b->mux));                                  \
                                                                       \
        return result;                                                 \
    }                                                                  \
                                                                       \
    void shared_buffer_artificial_signal_##BT(BT* b)                   \
    {                                                                  \
        pthread_cond_broadcast(&(b->not_anymore_full));                \
        pthread_cond_broadcast(&(b->not_anymore_empty));               \
    }                                                                  \



/**
 * Get the next item from the buffer. If buffer is empty, wait for a new item.
 * @param BT: the declared buffer type
 * @param T: the type of the items in the buffer
 * @param b: the buffer we want to get the item from
 * @param n: a NULL value for the type T
 * @return the item popped out from the buffer
 */
#define shared_buffer_get(BT, T, b, n) shared_buffer_get_##BT(b, n)

/**
 * Initialize the buffer
 * @param BT: the declared buffer type
 * @param T: the type of the items in the buffer
 * @param capacity: the capacity of the buffer
 * @return 
 *      the initialized buffer if everything's ok
 *      NULL otherwise
 */
#define shared_buffer_create(BT, T, capacity) shared_buffer_create_##BT(capacity)

/**
 * Put an item in the buffer. If buffer is full, wait.
 * @param BT: the declared buffer type
 * @param T: the type of the items in the buffer
 * @param b: the buffer we want to put the item in
 * @param x: the item
 */
#define shared_buffer_put(BT, T, b, x) shared_buffer_put_##BT(b, x)

/**
 * Destroy the buffer freeing the memory
 * @param BT: the declared buffer type
 * @param T: the type of the items in the buffer
 * @param b: the buffer we want to put the item in
 */
#define shared_buffer_free(BT, T, b) shared_buffer_free_##BT(b)

/**
 * Broadcast a signal to every waiting thread
 * @param BT: the declared buffer type
 * @param T: the type of the items in the buffer
 * @param b: the buffer instance
 */
#define shared_buffer_artificial_signal(BT, T, b) shared_buffer_artificial_signal_##BT(b)

#endif