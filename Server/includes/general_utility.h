#if !defined(_GENERAL_UTILITY_H)
#define _GENERAL_UTILITY_H

/*Equality Check*/
#define ec(s, r, c)         \
    if ((s) == (r)){ perror(#s); c; }
/*Not Equality Check*/
#define ec_n(s, r, c)       \
    if ((s) != (r)){ perror(#s); c; }

char isInteger(const char*, int*);

#endif