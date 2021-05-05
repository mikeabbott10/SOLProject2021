#include<stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<limits.h>

/*
    Overview: Checks if the evaluation of s is an integer and puts the value in n.
    Returns: 0 if s is an integer, 2 overflow or underflow detected,
             1 if s is not a number
    Params:
        s: char* to evaluate
        n: int* the evaluated value of s goes to
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