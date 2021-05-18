#if !defined(_CONNECTION_UTIL_H)
#define _CONNECTION_UTIL_H
#include<generic_shared_buffer.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include <sys/select.h>

#define MSG_LEN_LENGTH 9
#define FINE_REQ_RESPONSE "OK"
#define PENDING_REQ_RESPONSE "PR"
#define WRONG_FILEPATH_RESPONSE "WFP"
#define NOT_PERMITTED_ACTION_RESPONSE "NPA"

#define NO_FLAGS 0x00
#define O_CREATE 0x01
#define O_LOCK 0x02
#define OPEN 0x03
#define CLOSE 0x04
#define READ 0x05
#define READ_N 0x06
#define WRITE 0x07
#define APPEND 0x08
#define LOCK 0x09
#define UNLOCK 0x0a
#define REMOVE 0x0b

typedef struct msg {
    int len;
    char *content;
} msg_t;

/*client abstraction*/
typedef int client_fd_t;
void* clientBuffer; /*it will be a client_fd_buffer* */

// TODO
typedef struct{
    client_fd_t client_fd;
    void* opened_files; /*will be file_t* */
    void* locked_files; /*will be file_t* */
} client_t;

/*request abstraction*/
typedef struct{
    client_fd_t client_fd;

    char action; /* OPEN_FILE, CLOSE_FILE, READ_FILE, WRITE_FILE, APPEND_TO_FILE, LOCK_FILE, UNLOCK_FILE, REMOVE_FILE*/
    char* action_related_file_path; /*the file path the client wants to perform an action on*/
} request_t;

/*termination flag*/
#define HARD_QUIT 2
#define SOFT_QUIT 1
char quit_level = 0;
char globalQuit = 0;

/*used buffers*/
declare_shared_buffer_use(client_fd_t, client_fd_buffer);

pthread_t master_tid;
int master_progress = 0;
void master_clean_exit(int, int*);
int updatemax(fd_set, int);
int readn(long, void*, size_t);
int writen(long, void*, size_t);

int currentClientConnections = 0;

/*master/workers pipe*/
int pipefd[2];
/*int currentPipeMsgCount = 0;
char clientFDFromPipeToSet(fd_set*);
pthread_mutex_t pipe_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pipe_not_read_yet = PTHREAD_COND_INITIALIZER;*/

/*signal/master pipe*/
int signalPipefd[2];

/*workers util*/
pthread_t *worker_threads;
typedef void* (*workerFun)(void*);
char spawnWorkers(int, workerFun);
int sendTo(int, char*, int);
int getClientRequest(int clientFD, request_t* request);

#endif