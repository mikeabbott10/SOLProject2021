#if !defined(_CLIENT_API_H)
#define _CLIENT_API_H
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdint.h>
#include<errno.h>

/*Equality Check*/
#define ec(s, r, c)         \
    if ((s) == (r)){ perror(#s); c; }
/*Not Equality Check*/
#define ec_n(s, r, c)       \
    if ((s) != (r)){ perror(#s); c; }

int sockfd;

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

/**
 * Get current time in milliseconds
 * @return current time in milliseconds
 */
static inline uint64_t get_now_time() {
    struct timespec spec;
    if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
        return 0;
    }
    return spec.tv_sec * 1000 + spec.tv_nsec / 1e6;
}


/** Sleep for the requested number of milliseconds. 
 * @param msec: how many milliseconds we sleep for
*/
static inline void msleep(long msec){
    struct timespec ts;

    if (msec < 0){
        errno = EINVAL;
        return;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    nanosleep(&ts, NULL);
}

#endif