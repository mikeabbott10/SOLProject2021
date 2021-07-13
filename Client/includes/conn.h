#if !defined(_CONN_UTIL_H)
#define _CONN_UTIL_H

#define _POSIX_C_SOURCE 200809L
#include<general_utility.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include <unistd.h>
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

typedef struct msg {
    int len;
    char *content;
} msg_t;

int readn(long, void*, size_t);
int writen(long, void*, size_t);
int sendTo(int, char*, int);
int getServerMessage(int, char**);
msg_t buildMessage(char, char, const char*, char*, int);

#endif