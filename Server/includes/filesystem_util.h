#if !defined(_FILESYSTEM_UTIL_H)
#define _FILESYSTEM_UTIL_H
#include<connection_util.h>

typedef struct{
    char* path; /*the path of the file*/
    client_t* locked_by; /*the last client who has locked the file, NULL if file's not locked*/

    //_Bool open; // TODO: se open effettuata da un client per tutti
    //client_t* opened_by; // TODO: se open effettuata da un client per sè stesso

    char* content; /*the content of the file*/
    int length; /*the length of the file content*/
} file_t;

#endif