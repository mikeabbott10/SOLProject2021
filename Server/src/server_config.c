#define _POSIX_C_SOURCE 200809L /*for strdup and strtok_r*/
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include <general_utility.h>
#include <server_config.h>

/*
    Overview: closes a file descriptor
        From the manual:
        Many implementations always close the file descriptor (except in the 
        case of  EBADF,  meaning that  the  file  descriptor  was  invalid) 
        even if they subsequently report an error on return from close().
        POSIX.1 is currently silent on this point, but there are plans to 
        mandate this behavior in the next major release of the standard.
        
        On many implementations, close() must not be called again after 
        an EINTR error, and on at least one, close() must be called again.
    Returns: EXIT_FAILURE (even with no error)
    Param: fd the file descriptor
*/
char cleanup(FILE *fd){
    errno = 0;
    fclose(fd); // we do it just once
    if(errno!=0){
        printf("Warning: Error %d occurred while closing config file\n", errno);
    }
    return EXIT_FAILURE;
}

/*
    Overview: Reads one line from one file and stores it in a buffer
    Returns: the buffer or NULL
    Params:
        buffer: the buffer we store the line in
        len: the buffer length
        fp: the file pointer
*/
char* readLineFromFILE(char *buffer, unsigned int len, FILE *fp){
    char *buf = fgets(buffer, len, fp);
    if (buf == NULL)
        return buf;
    return buffer;
}

/*
    Overview: checks for config file line validity.
    Returns: EXIT_SUCCESS if everything's ok, EXIT_FAILURE if something wrong
    Params:
        line: the line of the file
        fd: the config file descriptor
*/
char checkConfigFileLine(char* line, int cnt){
    if(strchr(line, '\n') == NULL && line[0] != '#'){
        printf("Line %d is more than 250 characters. This is not allowed.\n", cnt);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
    Overview: updates one field of the server configuration structure (config_struct)
        From the manual:
        The strtok_s function differs from the POSIX strtok_r function 
        by guarding against storing outside of the string being tokenized, 
        and by checking runtime constraints. On correctly written programs, 
        though, the strtok_s and strtok_r behave the same.
    Returns: EXIT_SUCCESS if everything's ok, EXIT_FAILURE if line is invalid (key not recognized or invalid value)
    Param: line one line from the config file
*/
char populateConfigStruct(char* line){
    char *save = NULL;
    char *key, *value;
    key = strtok_r(line, "=", &save);
    value = strtok_r(NULL, "\n", &save);
    if(strncmp(key, "file_capacity", 13) == 0){
        if(isInteger(value, &(config_struct.file_capacity))!=0)
            return EXIT_FAILURE;
    }else if(strncmp(key, "byte_capacity", 13) == 0){
        if(isInteger(value, &(config_struct.byte_capacity))!=0)
            return EXIT_FAILURE;
    }else if(strncmp(key, "worker_threads", 14) == 0){
        if(isInteger(value, &(config_struct.worker_threads))!=0)
            return EXIT_FAILURE;
    }else if(strncmp(key, "socket_path", 11) == 0){
        config_struct.socket_path = strdup(value); /*needs to be freed*/
    }else
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

/*
    Overview: Parses the config file and populates the config struct
    param: config file path
*/
char parseConfigFile(char* configFilePath){
    int bufSize = 256;
    char returnVal = EXIT_SUCCESS;

    FILE* fdi = fopen(configFilePath, "r");
    esc(fdi, NULL, "Opening input file", return EXIT_FAILURE);

    char *buffer = malloc(bufSize);
    esc(buffer, NULL, "Allocation went bad", return cleanup(fdi));

    int i = 0; /*counts the lines*/
    while(readLineFromFILE(buffer, bufSize, fdi) != NULL){
        i+=1;
        if(checkConfigFileLine(buffer, i)==EXIT_FAILURE){
            returnVal = EXIT_FAILURE;
            break;
        }
        if(strncmp(buffer, "#end_of_config",14) == 0)
            break;
        if(populateConfigStruct(buffer) == EXIT_FAILURE){ 
            printf("Error. Config file: Invalid key or value at line %d\n", i);
            returnVal = EXIT_FAILURE;
            break;
        }
    }
    cleanup(fdi);
    free(buffer);
    return returnVal;
}