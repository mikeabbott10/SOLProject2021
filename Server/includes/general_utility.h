#if !defined(_GENERAL_UTILITY_H)
#define _GENERAL_UTILITY_H

/*Equality Check*/
#define ec(s, r, c)         \
    if ((s) == (r)){ perror(#s); c; }
/*Not Equality Check*/
#define ec_n(s, r, c)       \
    if ((s) != (r)){ perror(#s); c; }

#define SET_MAX(max,n) max = (n > max) ? n : max

char isInteger(const char*, int*);
int max(int, ...);
char* intToStr(int, int);

#endif