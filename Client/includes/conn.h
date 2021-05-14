#if !defined(_CONN_UTIL_H)
#define _CONN_UTIL_H

#define _POSIX_C_SOURCE 200809L
#include<general_utility.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include <unistd.h>
#include<string.h>

typedef struct msg {
    int len;
    char *str;
} msg_t;

int readn(long, void*, size_t);
int writen(long, void*, size_t);
int sendStringTo(int, char*);
int getServerMessage(int, char**);

#endif