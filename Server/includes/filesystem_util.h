#if !defined(_FILESYSTEM_UTIL_H)
#define _FILESYSTEM_UTIL_H

/*file abstraction*/
typedef struct{
    char* path; /*the path of the file*/
    client_fd_t locked_by; /*the last client who has locked the file, NULL if file's not locked*/
    client_fd_t* opened_by; /*the clients who opened the file*/
    
    char* content; /*the content of the file*/
    int length; /*the length of the file content*/
} file_t;

#endif