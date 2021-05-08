#if !defined(_CONNECTION_UTIL_H)
#define _CONNECTION_UTIL_H
#include<stdio.h>
#include<pthread.h>

/*client abstraction*/
typedef struct{
    int fd; /*client descriptor*/
    
    int next_msg_length; /*length of the next message to receive from the client, -1 if no message incoming*/
    /*TODO: THINK THIS IS USELESS -> */char* message; /*the last message the client sent to the server*/

    int action; /* NONE, OPENING_FILE, CLOSING_FILE, READING_FILE, WRITING_FILE, APPENDING_TO_FILE, LOCKING_FILE, UNLOCKING_FILE, REMOVING_FILE*/
    char* action_related_file_path; /*the file path the client wants to perform an action on*/
} client_t;

/* Implements a client FIFO buffer */
typedef struct {
    client_t* buffer; /*the buffer of clients*/

    size_t capacity; /*capacity of the buffer*/
    size_t start; /*index of the first valid item*/
    size_t end; /*index of the last item*/

    pthread_mutex_t mux; /*access to the buffer in mutual exclusion*/
    pthread_cond_t not_anymore_empty; /*condition variable for buffer emptiness*/
    pthread_cond_t not_anymore_full; /*condition variable for buffer fullness*/
} client_buffer_t;

client_buffer_t* client_buffer_create(size_t);
void client_buffer_free(client_buffer_t*);
void client_buffer_put(client_buffer_t*, client_t);
client_t client_buffer_get(client_buffer_t*);

client_buffer_t* clientBuffer = NULL;

#endif