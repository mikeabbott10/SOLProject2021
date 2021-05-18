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
    if(n<0 || n>pow(10,strLen)){
        puts("intToStr: n not valid for conversion");
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
    struct timespec ts;

    if (msec < 0){
        errno = EINVAL;
        return;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    nanosleep(&ts, NULL);
}