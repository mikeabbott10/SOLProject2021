#if !defined(_CLIENT_API_H)
#define _CLIENT_API_H
#include <time.h>
#include <stdint.h>
#include<errno.h>

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

int sockfd;
char* sockPath = NULL;

int openConnection(const char*, int, const struct timespec);
int closeConnection(const char*);
int openFile(const char*, int);
int readFile(const char*, void**, size_t*);
int readNFiles(int, const char*);
int writeFile(const char*, const char*);
int appendToFile(const char*, void*, size_t, const char*);
int lockFile(const char*);
int unlockFile(const char*);
int closeFile(const char*);
int removeFile(const char*);


#endif