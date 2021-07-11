#include<general_utility.h>
#include<stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<limits.h>
#include <stdarg.h>
#include <time.h>
#include<math.h>

/**
 * Checks if the evaluation of s is an integer and puts the value in n.
 * @param s: char* to evaluate
 * @param n: int* the evaluated value of s goes to
 * @return 0 if s is an integer, 2 overflow or underflow detected, 1 if s is not a number
 */
char isInteger(const char* s, int* n){
    char *e = NULL;
    errno = 0;
    long val = strtol(s, &e, 10);

    if (errno == ERANGE || val > INT_MAX || val < INT_MIN){
        // overflow/underflow
        return 2;
    }

    if (errno==0 && e != NULL && e != s){
        *n = (int) val;
        // è un numero valido
        return 0;
    }

    // non è un numero
    return 1;
}

/**
 * Checks if the evaluation of s is a size_t and puts the value in n.
 * @param s: char* to evaluate
 * @param n: size_t* the evaluated value of s goes to
 * @return 0 if s is a size_t, 2 overflow or underflow detected, 1 if s is not a size_t
 */
char isSizeT(const char* s, size_t* n){
    char *e = NULL;
    errno = 0;
    long val = strtol(s, &e, 10);

    if (errno == ERANGE){
        // overflow/underflow
        return 2;
    }

    if(val < 0)
        return 1;

    if (errno==0 && e != NULL && e != s){
        *n = (size_t) val;
        // è un size_t valido
        return 0;
    }

    // non è un numero
    return 1;
}

/**
 * Find maximum between two or more integer variables
 * @param args Total number of integers
 * @param ... List of integer variables to find maximum
 * @return Maximum among all integers passed
 */
int max(int args, ...){
    int i, max, cur;
    va_list valist;
    va_start(valist, args);
    
    max = INT_MIN;
    
    for(i=0; i<args; i++){
        cur = va_arg(valist, int); // Get next elements in the list
        if(max < cur)
            max = cur;
    }
    
    va_end(valist); // Clean memory assigned by valist
    
    return max;
}

/**
 * Allocate one string and write the value of n (>=0) in it. The returned string has strLen characters.
 * @param n: the number > 0
 * @param strLen: the final length of the generated string
 * @return the string or NULL
 */ 
char* intToStr(int n, int strLen){
    if(n<0 || n>=pow(10,strLen)){
        //puts("intToStr: n not valid for conversion");
        errno = EINVAL;
        return NULL;
    }
    int length = snprintf( NULL, 0, "%0*d", strLen, n);
    char* str = malloc( length + 1 );
    if(str==NULL) return NULL;
    snprintf( str, length + 1, "%0*d", strLen, n );
    return str;
}

/**
 * Get current time in milliseconds
 * @return current time in milliseconds
 */
uint64_t get_now_time() {
    struct timespec spec;
    if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
        return 0;
    }
    return spec.tv_sec * 1000 + spec.tv_nsec / 1e6;
}


/** Sleep for the requested number of milliseconds. 
 * @param msec: how many milliseconds we sleep for
*/
void msleep(long msec){
    if(msec==0)
        return;
    struct timespec ts;

    if (msec < 0){
        errno = EINVAL;
        return;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    nanosleep(&ts, NULL);
}

//---------- FILE UTILS --------------------------------------------------


DIR * openAndCD(char * path){
    ec(chdir(path), -1, return NULL); // mi sposto nella cartella
    DIR *newdir = opendir(".");
    ec(newdir, NULL, return NULL);
    return newdir;
} 

int getAbsolutePath(const char *path, char **absPathPtr){
    if (!path)
        return -1;
    if ( (*path) == '/' ){
        ec( *absPathPtr = strdup(path), NULL, return -1 );
        return 0;
    }
    char cwd[MAX_PATH_LEN];
    ec( getcwd(cwd, MAX_PATH_LEN), NULL, return -1);
    ec(*absPathPtr = calloc(strlen(path) + strlen(cwd) + 2, sizeof(char)), NULL, return -1);
    if( snprintf(*absPathPtr, MAX_PATH_LEN, "%s/%s", cwd, path) < 0 )
        return -1;
    return 0;
}

/**
 * @return 
 *      1 fatal error
 *      0 success
 */
int getFilePath(char** dirPath, char* fileName){
    size_t dirPathLen = strlen(*dirPath);
    size_t filePathLen = strlen(fileName);
    ec( *dirPath = realloc(*dirPath, dirPathLen+filePathLen+1), NULL, return 1);
    memmove((*dirPath)+dirPathLen, fileName, filePathLen+1); // move the '\0' too
    return 0;
}

int writeLocalFile(char* filePath, char* content, int contentSize){
    FILE* fdo = fopen(filePath, "w");
    ec( fdo, NULL, return -1; );
    ec( fwrite(content, sizeof(char), contentSize, fdo), -1, 
        ec(fclose(fdo), -1, return -1); return -1;
    );
    // user space buffer flush
    ec(fflush(fdo), -1, return -1); 
    // kernel space buffer flush
    ec(fsync(fileno(fdo)), -1, return -1);
    ec(fclose(fdo), -1, return -1);
    return 0;
}
