#if !defined(_GENERAL_UTILITY_H)
#define _GENERAL_UTILITY_H
#define _POSIX_C_SOURCE 200809L
#include<pthread.h>
#include<assert.h>
#include<string.h>

#define MSG_LEN_LENGTH 9
#define FINE_REQ_RESPONSE "OK"
#define LOCKED_FILE_REMOVED "LFR"
#define WRONG_FILEPATH_RESPONSE "WFP"
#define NOT_PERMITTED_ACTION_RESPONSE "NPA"
#define EVICTED_FILE_CONTENT "EFC"
#define READ_FILE_CONTENT "RFF"
#define NO_CONTENT_FILE "NCF"
#define CONTENT_TOO_LARGE "CTL"

/*Equality Check*/
#define ec(s, r, c)         \
    if ((s) == (r)){ perror(#s); c; }
/*Not Equality Check*/
#define ec_n(s, r, c)       \
    if ((s) != (r)){ perror(#s); c; }

#define SET_MAX(max,n) max = (n > max) ? n : max

char isInteger(const char*, int*);
int checkMultiplicationOverflow(int, int);
int max(int, ...);
char* intToStr(int, int);
int safe_mutex_lock(pthread_mutex_t*);
int safe_mutex_unlock(pthread_mutex_t*);
int readn(long, void*, size_t);
int writen(long, void*, size_t);
int sendTo(int, char*, int);


/*----------- connection_util and filesystem_util common stuff-------------------------------------*/
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

typedef int client_fd_t;

/*node of client fds list*/
typedef struct cl{
    client_fd_t client_fd;
    struct cl* next;
} client_t;

void getNullMessage(msg_t *msg);
int buildMsg(msg_t*, char*);

/*----------- termination stuff --------------------------------------------------------------------*/
/*termination flag*/
#define HARD_QUIT 2
#define SOFT_QUIT 1

// global termination value (no need to sync the access bc we only have 
// just 1 writer who writes just once)
char globalQuit;

#endif