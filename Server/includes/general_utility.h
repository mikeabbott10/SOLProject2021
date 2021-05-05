#if !defined(_GENERAL_UTILITY_H)
#define _GENERAL_UTILITY_H

/*Equality (System or library function call) Check*/
#define esc(s, r, m, c)         \
    if ((s) == (r)){ perror(m); c; }
/*Not Equality (System or library function call) Check*/
#define esc_n(s, r, m, c)       \
    if ((s) != (r)){ perror(m); c; }

/*Equality Check*/
#define ec(s, r, fm, c)         \
    if ((s) == (r)){ fm; c; }
/*Not Equality Check*/
#define ec_n(s, r, fm, c)       \
    if ((s) != (r)){ fm; c; }


char isInteger(const char*, int*);

#endif