#if !defined(_FILESYSTEM_UTIL_H)
#define _FILESYSTEM_UTIL_H

/*file abstraction*/
typedef struct{
    char* path; /*the path of the file*/
    void* locked_by; /*will be client_t : the last client who has locked the file, NULL if file's not locked*/
    void* opened_by; /*will be client_t* : the clients who opened the file*/
    
    char* content; /*the content of the file*/
    int length; /*the length of the file content*/
} file_t;

#endif