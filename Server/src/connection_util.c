#include<connection_util.h>
#include<general_utility.h>
#include<stdlib.h>
#include<errno.h>

/*
    Overview: Safe mutex lock
    Param: the mutex to lock
*/
void safe_mutex_lock(pthread_mutex_t* mux){
    int err;
    if ((err = pthread_mutex_lock(mux)) != 0){
        errno = err;
        perror("lock");
        pthread_exit(NULL);
    }/*else{
        //printf("locked\n");
    }*/
}

/*
    Overview: Safe mutex unlock
    Param: the mutex to unlock
*/
void safe_mutex_unlock(pthread_mutex_t* mux){
    int err;
    if ((err = pthread_mutex_unlock(mux)) != 0){
        errno = err;
        perror("unlock");
        pthread_exit(NULL);
    }/*else{
        //printf("unlocked\n");
    }*/
}

/*
    Overview: Initializes and allocates memory for the client buffer
    Returns: pointer to the created client buffer, NULL if not enough memory
    Param: capacity of the client buffer
*/
client_buffer_t* client_buffer_create(size_t capacity){
    client_buffer_t* buf;
    ec(buf = (client_buffer_t*)malloc(sizeof(client_buffer_t)), NULL, return NULL);
    buf->capacity = capacity + 1;
    buf->start = 0;
    buf->end = 0;
    if((buf->buffer = (client_t*)malloc((capacity + 1) * sizeof(client_t))) == NULL){
        free(buf);
        buf = NULL;
    }else{
        pthread_mutex_init(&(buf->mux), NULL);
        pthread_cond_init(&(buf->not_anymore_empty), NULL);
        pthread_cond_init(&(buf->not_anymore_full), NULL);
    }
    return buf;
}

/*
    Overview: Frees the allocated memory for the client buffer
    Param: pointer to the client buffer
*/
void client_buffer_free(client_buffer_t* buf){
    free(buf->buffer);
    pthread_mutex_destroy(&(buf->mux));
    pthread_cond_destroy(&(buf->not_anymore_empty));
    pthread_cond_destroy(&(buf->not_anymore_full));
    free(buf);
}

/*
    Overview: Pushes a new client c to the end of the client buffer
    Params: 
        buf: pointer to the client buffer
        c: the new client
*/
void client_buffer_put(client_buffer_t* buf, client_t c){
    safe_mutex_lock(&(buf->mux));
    while (buf->start == (buf->end + buf->capacity + 1) % buf->capacity) {
        pthread_cond_wait(&(buf->not_anymore_full), &(buf->mux));
    }
    buf->buffer[buf->end] = c; /*we put the item in the tail, at the end*/
    ++(buf->end);
    buf->end %= buf->capacity;
    pthread_cond_signal(&(buf->not_anymore_empty));
    safe_mutex_unlock(&(buf->mux));
}

/*
    Overview: Pops the oldest client, not popped yet, from the client buffer
    Returns: The popped client
    Param: pointer to the client buffer
*/
client_t client_buffer_get(client_buffer_t* buf){
    client_t result;
    safe_mutex_lock(&(buf->mux));
    while (buf->start == buf->end) {
        pthread_cond_wait(&(buf->not_anymore_empty), &(buf->mux));
    }
    result = buf->buffer[buf->start]; /*we get the first valid item from the buffer*/
    ++(buf->start);
    buf->start %= buf->capacity;
    pthread_cond_signal(&(buf->not_anymore_full));
    safe_mutex_unlock(&(buf->mux));
    return result;
}