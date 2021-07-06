#if !defined(_CONNECTION_UTIL_H)
#define _CONNECTION_UTIL_H
#include<generic_shared_buffer.h>
#include<general_utility.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include <sys/select.h>

void* clientBuffer; /*it will be a client_fd_buffer* */

/*request abstraction*/
typedef struct{
    client_fd_t client_fd;

    char action; /* OPEN, CLOSE, READ, READ_N, WRITE, 
                    APPEND, LOCK, UNLOCK, REMOVE */
    char action_flags; /*NO_FLAGS, O_CREATE, O_LOCK*/
    char* action_related_file_path; /*the file path the client wants to perform an action on*/
    char* content; /*the content of the file, if needed*/
} request_t;

/*used buffers*/
declare_shared_buffer_use(client_fd_t, client_fd_buffer);

pthread_t master_tid;
int master_progress = 0;
void master_clean_exit(int, int*);
int updatemax(fd_set, int);

int currentClientConnections = 0;

/*master/workers pipe*/
int pipefd[2];

/*signal/master pipe*/
int signalPipefd[2];

/*workers util*/
pthread_t *worker_threads;
typedef void* (*workerFun)(void*);
char spawnWorkers(int, workerFun);
void getNullMessage(msg_t*);
int getClientRequest(int clientFD, request_t* request);
int parseMessage(char*, int, request_t*);
int performActionAndGetResponse(request_t, msg_t*);

#endif