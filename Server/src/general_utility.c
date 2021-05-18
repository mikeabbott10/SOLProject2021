#include<stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<limits.h>
#include <stdarg.h>

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
 * Allocate one string and write the value of n in it. The returned string has 9 characters.
 * @param n: the number
 * @return the string or NULL
 */ 
char* intToStr9(int n){
    int length = snprintf( NULL, 0, "%09d", n);
    char* str = malloc( length + 1 );
    if(str==NULL) return NULL;
    snprintf( str, length + 1, "%09d", n );
    return str;
}