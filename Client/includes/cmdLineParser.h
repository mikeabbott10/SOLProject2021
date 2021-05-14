#if !defined(_CMDLINEPARSER_H)
#define _CMDLINEPARSER_H

/*represents a list of operations/files/directories*/
typedef struct{
    char *f_socketpath;
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

void initML(mainList_t*);
int parseCmdLine(int, char**, mainList_t*);
void print_usage(char*);
char storeArgs(char ***, char *, int*);
void freeList(mainList_t);
void freeStuff(char **, int);
char isOption(char*);

#endif