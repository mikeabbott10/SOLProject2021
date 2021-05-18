#if !defined(_GENERAL_UTILITY_H)
#define _GENERAL_UTILITY_H
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>

/*Equality Check*/
#define ec(s, r, c)         \
    if ((s) == (r)){ perror(#s); c; }
/*Not Equality Check*/
#define ec_n(s, r, c)       \
    if ((s) != (r)){ perror(#s); c; }

char isInteger(const char*, int*);
int max(int, ...);
char* intToStr(int,int);
uint64_t get_now_time();
void msleep(long);

#endif