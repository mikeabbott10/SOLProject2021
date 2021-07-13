#if !defined(_CMDLINEPARSER_H)
#define _CMDLINEPARSER_H
#include<general_utility.h>

void initML(mainList_t*);
int parseCmdLine(int, char**, mainList_t*);
void print_usage(char*);
char storeArgs(char ***, char *, int*);
void freeList(mainList_t);
void freeStuff(char **, int);
char isOption(char*);

#endif