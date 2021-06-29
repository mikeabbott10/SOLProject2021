#define _POSIX_C_SOURCE 200809L
#include<cmdLineParser.h>
#include<general_utility.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>

/**
 * Parse the command line and store the gotten values in a given mainList_t
 * @param argc: the number of items in argv
 * @param argv: the command line options and arguments array
 * @param ml: the mainList_t
 * @return: 0 if everything's ok, 1 if parsing fails
 */ 
int parseCmdLine(int argc, char**argv, mainList_t *ml){
    /*parse command line*/
    char option = 0;
    opterr = 0;
    while ((option = getopt(argc, argv, "f:W:D:r:l:u:c:d:hR::t::p::w:")) != -1){
        /* Note: we can get optional arguments (::) if there is no space 
        between the option and the argument ( -R5 gets 5 as arg of -R )*/
        switch (option){
            case 'f':/*Sets the socket file path up to the specified socketPath*/
                if(ml->f_socketpath != NULL){
                    fprintf(stderr, "Option -f is specified multiple times.\n");
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                ec( ml->f_socketpath = malloc(strlen(optarg)+1), NULL, return EXIT_FAILURE );
                strcpy(ml->f_socketpath, optarg);
                break;
            case 'W':/*Sends to the server the specified files*/
                ec( storeArgs(&(ml->W_filenames), optarg, &(ml->W_itemsCount)), EXIT_FAILURE, return EXIT_FAILURE );
                break;
            case 'D':/*If capacity misses occur on the server, save the files it gets to us in the specified dirname directory*/
                if(ml->D_dirname != NULL){
                    fprintf(stderr, "Option -D is specified multiple times.\n");
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                ec( ml->D_dirname = malloc(strlen(optarg)+1), NULL, return EXIT_FAILURE );
                strcpy(ml->D_dirname, optarg);
                break;
            case 'r':/*Reads the specified files from the server*/
                ec( storeArgs(&(ml->r_filenames), optarg, &(ml->r_itemsCount)), EXIT_FAILURE, return EXIT_FAILURE );
                break;
            case 'l':/* Locks the specified files */
                ec( storeArgs(&(ml->l_filenames), optarg, &(ml->l_itemsCount)), EXIT_FAILURE, return EXIT_FAILURE );
                break;
            case 'u':/* Unlocks the specified files */
                ec( storeArgs(&(ml->u_filenames), optarg, &(ml->u_itemsCount)), EXIT_FAILURE, return EXIT_FAILURE );
                break;
            case 'c':/* Removes the specified files from the server */
                ec( storeArgs(&(ml->c_filenames), optarg, &(ml->c_itemsCount)), EXIT_FAILURE, return EXIT_FAILURE );
                break;
            case 'd':/* Saves files read with -r or -R option in the specified dirname directory */
                if(ml->d_dirname != NULL){
                    fprintf(stderr, "Option -d is specified multiple times.\n");
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                ec( ml->d_dirname = malloc(strlen(optarg)+1), NULL, return EXIT_FAILURE );
                strcpy(ml->d_dirname, optarg);
                break;
            case 'R':/* Reads n random files from the server (if n=0, reads every file) */
                ml->R_n = 0;
                if(optarg!=NULL){
                    if( isInteger(optarg, &(ml->R_n)) != 0 ){
                        fprintf(stderr, "-R option requires an integer.\n");
                        print_usage(argv[0]);
                        return EXIT_FAILURE;
                    }
                }
                break;
            case 't':/* Specifies the time between two consecutive requests to the server */
                if(ml->t_time != 0){
                    fprintf(stderr, "Option -t is specified multiple times.\n");
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                if(optarg!=NULL){
                    if( isInteger(optarg, &(ml->t_time)) != 0 ){
                        fprintf(stderr, "-t option requires an integer.\n");
                        print_usage(argv[0]);
                        return EXIT_FAILURE;
                    }
                }
                break;
            case 'p':/* Prints information about operation performed on the server */
                if(ml->p == 1){
                    fprintf(stderr, "Option -p is specified multiple times.\n");
                    print_usage(argv[0]);
                    return EXIT_FAILURE;
                }
                ml->p = 1;
                break;
            case 'w':/* Sends to the server n files from the specified dirname directory. If n=0, sends every file in dirname */
                {
                    char *save = NULL;
                    char *item;
                    char *bkp;
                    ec( bkp = malloc(strlen(optarg)+1), NULL, return EXIT_FAILURE );
                    strcpy(bkp, optarg);
                    item = strtok_r(bkp, ",", &save);

                    ec( ml->w_dirname = malloc(strlen(item)+1), NULL, return EXIT_FAILURE );
                    strcpy(ml->w_dirname, item);

                    item = strtok_r(NULL, ",", &save);
                    if(isInteger(item, &(ml->w_n)) != 0){
                            fprintf(stderr, "-w option requires a path and (optionally) an integer.\n");
                            print_usage(argv[0]);
                            return EXIT_FAILURE;
                    }
                    free(bkp);
                    break;
                }
            case '?':
                fprintf(stderr, "Unknown option '-%c' or '-%c' requires an argument.\n", optopt, optopt);
            case 'h':
                print_usage(argv[0]);
                return EXIT_FAILURE;
            default:
                print_usage(argv[0]); 
                return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/**
 * Free the allocated properties of ml
 * @param ml: the mainList_t
 */ 
void freeList(mainList_t ml){
    /* print main list*/
    /* printf("mainList:\n"); */
    if(ml.f_socketpath!=NULL){ 
        /* printf("char *f_socketpath -> %s\n", ml.f_socketpath); */
        free(ml.f_socketpath);
    }
    if(ml.w_dirname!=NULL) {
        /* printf("char *w_dirname -> %s\n", ml.w_dirname);
        printf("int w_n -> %d\n", ml.w_n); */
        free(ml.w_dirname);
    }
    if(ml.D_dirname!=NULL) {
        /* printf("char *D_dirname -> %s\n", ml.D_dirname);
        printf("int R_n -> %d\n", ml.R_n); */
        free(ml.D_dirname);
    }
    if(ml.d_dirname!=NULL){
        /* printf("char* d_dirname -> %s\n", ml.d_dirname); */
        free(ml.d_dirname);
    }
        /* printf("int t_time -> %d\n", ml.t_time);
        printf("char p -> %d\n", ml.p); */

    if(ml.W_filenames!=NULL){
        /* printf( "char **W_filenames -> "); */
        freeStuff(ml.W_filenames, ml.W_itemsCount);
    }
    if(ml.l_filenames!=NULL){
        /* printf( "char **l_filenames -> "); */
        freeStuff(ml.l_filenames, ml.l_itemsCount);
    }
    if(ml.u_filenames!=NULL){
        /* printf( "char **u_filenames -> "); */
        freeStuff(ml.u_filenames, ml.u_itemsCount);
    }
    if(ml.c_filenames!=NULL){
        /* printf( "char **c_filenames -> "); */
        freeStuff(ml.c_filenames, ml.c_itemsCount);
    }
    if(ml.r_filenames!=NULL){
        /* printf( "char **r_filenames -> "); */
        freeStuff(ml.r_filenames, ml.r_itemsCount);
    }
}

/**
 * Free what's inside str_arr and the str_arr itself
 * @param str_arr: the array to free
 * @param i_max: the length of the str_arr
 */ 
void freeStuff(char **str_arr, int i_max){
    int i = 0;
    while(str_arr != NULL && i<i_max){
        /* printf("%s , ", str_arr[i]); */
        free(str_arr[i++]);
    }
    if(str_arr!=NULL) free(str_arr);
    /* puts(""); */
}

/**
 * Store arg in *str_arr_ptr. arg is a list of comma-separated strings.
 * @param str_arr_ptr: pointer to the string array we want to store items in
 * @param arg: the comma-separated strings
 * @param itemCount: pointer to the current length of *str_arr_ptr. It will be modified adding items to the array.
 * @return: 0 if everything's ok, 1 if parsing fails
 */ 
char storeArgs(char ***str_arr_ptr, char *arg, int *itemCount){
    char *save = NULL;
    char *item;
    char *bkp;
    ec( bkp = malloc(strlen(arg)+1), NULL, return EXIT_FAILURE );
    strcpy(bkp, arg);
    item = strtok_r(bkp, ",", &save);
    while(item){
        *str_arr_ptr = realloc( *str_arr_ptr, (++(*itemCount))*sizeof(char*) );
        if(*str_arr_ptr == NULL){
            /* not enough memory */
            free(*str_arr_ptr);
            free(bkp);
            return EXIT_FAILURE;
        }
        (*str_arr_ptr)[(*itemCount)-1] = malloc(strlen(item)+1);
        if((*str_arr_ptr)[(*itemCount)-1] == NULL){
            /* not enough memory */
            free(*str_arr_ptr);
            free(bkp);
            return EXIT_FAILURE;
        }
        strcpy((*str_arr_ptr)[(*itemCount)-1], item);
        item = strtok_r(NULL, ",", &save);
    }
    free(bkp);
    return EXIT_SUCCESS;
}

/**
 * Print the usage of this program
 * @param exe: the name of the executable
 */ 
void print_usage(char *exe){
    printf( "Usage: %s -h\n"
            "Usage: %s -f socketPath [-w dirname [,n=0]] [-Dd dirname] [-p]"
            "[-t [0]] [-R [,n=0]] [-Wrluc file1 [,file2,file3 ...]]\n"
            "\nPlease note that if you specify an optional argument, there should be no space between it and its option.\n\n"
            "Options:\n"
            "  -h,\tPrints this message\n"
            "  -f,\tSets the socket file path up to the specified socketPath\n"
            "  -w,\tSends to the server n files from the specified dirname directory. If n=0, sends every file in dirname\n"
            "  -W,\tSends to the server the specified files\n"
            "  -D,\tIf capacity misses occur on the server, save the files it gets to us in the specified dirname directory\n"
            "  -r,\tReads the specified files from the server\n"
            "  -R,\tReads n random files from the server (if n=0, reads every file)\n"
            "  -d,\tSaves files read with -r or -R option in the specified dirname directory\n"
            "  -t,\tSpecifies the time between two consecutive requests to the server\n"
            "  -l,\tLocks the specified files\n"
            "  -u,\tUnlocks the specified files\n"
            "  -c,\tRemoves the specified files from the server\n" 
            "  -p,\tPrints information about operation performed on the server\n", 
        exe,exe
    );
}

/**
 * Tells if a string s is an option in this context or is not.
 * @param s: the string to check
 * @return: 1 if s is an option, 0 otherwise
 */ 
char isOption(char* s){
    if(s == NULL || strnlen(s, 2) != 2 || s[0]!='-') return 0; /*not an option*/
    char *options = calloc(14, sizeof(char));
    strncpy(options, "hfwWDrRdtlucp", 14);
    if(strchr(options, s[1])==NULL){
        free(options);
        return 0;
    }else{
        free(options);
        return 1;
    }
}

/**
 * Init a mainList_t
 * @param ml: the mainList_t
 */ 
void initML(mainList_t *ml){
    ml->f_socketpath = NULL;
    ml->D_dirname = NULL;
    ml->d_dirname = NULL;
    ml->w_dirname = NULL;
    ml->c_filenames = NULL;
    ml->W_filenames = NULL;
    ml->r_filenames = NULL;
    ml->l_filenames = NULL;
    ml->u_filenames = NULL;
    ml->c_itemsCount = 0;
    ml->W_itemsCount = 0;
    ml->r_itemsCount = 0;
    ml->l_itemsCount = 0;
    ml->u_itemsCount = 0;
    ml->p = 0;
    ml->R_n = -1;
    ml->t_time = 0;
    ml->w_n = 0;
}