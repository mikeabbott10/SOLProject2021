#if !defined(_GENERAL_UTILITY_H)
#define _GENERAL_UTILITY_H
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include<time.h>

/*represents a list of operations/files/directories*/
typedef struct{
    char *f_socketpath; //
    char *w_dirname; 
    int w_n;
    char **W_filenames;
    int W_itemsCount;
    char *D_dirname;
    char **r_filenames;
    int r_itemsCount;
    int R_n;
    char* d_dirname;
    int t_time;
    char **l_filenames;
    int l_itemsCount;
    char **u_filenames;
    int u_itemsCount;
    char **c_filenames;
    int c_itemsCount;
    char p;
} mainList_t;

#define MAX_PATH_LEN 4096

/*Equality Check*/
#define ec(s, r, c)         \
    if ((s) == (r)){ perror(#s); c; }
/*Not Equality Check*/
#define ec_n(s, r, c)       \
    if ((s) != (r)){ perror(#s); c; }

char operationRes[MAX_PATH_LEN*3]; // just to be sure

#define PRINT_INFO(opStr,absPath)                                       \
    if( snprintf(operationRes, MAX_PATH_LEN*2,                          \
            "%s %s: %s", opStr, absPath, strerror(errno)) < 0 )         \
        return -1;                                                      \
    printf("%s\n", operationRes);                                       \

//----- FILE UTILS ---------------------------------------------------
DIR * openAndCD(char * );
int getAbsolutePath(const char *, char **);
int getFilePath(char** , char* );
int writeLocalFile(char* , char* , int );
int getFileContent(const char*, char**, size_t*);


char isInteger(const char*, int*);
char isSizeT(const char*, size_t*);
int max(int, ...);
char* intToStr(int,int);
uint64_t get_now_time();
void msleep(long);

#endif