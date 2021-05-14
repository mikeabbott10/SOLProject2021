#include<client_API.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* ind AF_UNIX */
#define UNIX_PATH_MAX 108 /* man 7 unix */

/**
 * Open an AF_UNIX connection to the socket file sockname.
 * @param sockname: the socket path
 * @param msec: waiting time after a failed connection request
 * @param abstime: won't try to connect again after abstime
 * @return: 0 if a connection is established, -1 if something wrong (errno is setted up)
 */
int openConnection(const char* sockname, int msec, const struct timespec abstime){
    ec( sockfd = socket(AF_UNIX, SOCK_STREAM, 0), -1, return EXIT_FAILURE );

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, sockname, strlen(sockname)+1);

    /*get how many ms we can try on*/
    uint64_t stop_abs_ms = abstime.tv_sec * 1000 + abstime.tv_nsec / 1e6;
    uint64_t abs_ms = get_now_time();
    int64_t remaining_ms = stop_abs_ms - abs_ms;
     
    errno = 0;
    int res;
    while ( (res=connect(sockfd,(struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1 && remaining_ms > 0) { /*keep trying*/
        msleep(msec);
        abs_ms = get_now_time();
        remaining_ms = stop_abs_ms - abs_ms;
        /*printf("remains: %ld\t", remaining_ms);*/
    }
    return res;
}
/*
int closeConnection(const char* sockname){

}

int openFile(const char* pathname, int flags){

}

int readFile(const char* pathname, void** buf, size_t* size){

}

int readNFiles(int N, const char* dirname){

}

int writeFile(const char* pathname, const char* dirname){

}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){

}

int lockFile(const char* pathname){

}

int unlockFile(const char* pathname){

}

int closeFile(const char* pathname){
    
}

int removeFile(const char* pathname){
    
}*/