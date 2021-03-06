#define _POSIX_C_SOURCE 200809L
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include <general_utility.h>
#include <server_config.h>

/**
 * Close a file descriptor
 * @param fd: the file descriptor
 * @return EXIT_FAILURE (even with no error)
 */
char fd_cleanup(FILE *fd){
    /*From the manual:
     *  Many implementations always close the file descriptor (except in the 
     *  case of  EBADF,  meaning that  the  file  descriptor  was  invalid) 
     *  even if they subsequently report an error on return from close().
     *  POSIX.1 is currently silent on this point, but there are plans to 
     *  mandate this behavior in the next major release of the standard.
     *     
     *  On many implementations, close() must not be called again after 
     *  an EINTR error, and on at least one, close() must be called again.
    */
    errno = 0;
    ec_n(fclose(fd), 0,); // we do it just once
    return EXIT_FAILURE;
}

/**
 * Read one line from one file and stores it in a buffer
 * @param buffer: the buffer we store the line in
 * @param len: the buffer length
 * @param fp: the file pointer
 * @return the buffer, or NULL
 */
char* readLineFromFILE(char *buffer, unsigned int len, FILE *fp){
    ec(fgets(buffer, len, fp), NULL, return NULL);
    return buffer;
}

/**
 * Check for config file line validity.
 * @param line: the line of the file
 * @param fd: the config file descriptor
 * @return EXIT_SUCCESS if everything's ok, 
 *         EXIT_FAILURE if something goes wrong
 */
char checkConfigFileLine(char* line, int cnt){
    if(strchr(line, '\n') == NULL && line[0] != '#'){
        printf("Line %d is more than 250 characters. This is not allowed.\n", cnt);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * Update one field of the server configuration structure (config_struct)
 * @param line: one line from the config file
 * @return EXIT_SUCCESS if everything's ok, 
 *         EXIT_FAILURE if line is invalid (key not recognized or invalid value)
 */
char populateConfigStruct(char* line){
    char *save = NULL;
    char *key, *value;
    /*From the manual:
     *  The strtok_s function differs from the POSIX strtok_r function 
     *  by guarding against storing outside of the string being tokenized, 
     *  and by checking runtime constraints. On correctly written programs, 
     *  though, the strtok_s and strtok_r behave the same.
    */
    key = strtok_r(line, "=", &save);
    value = strtok_r(NULL, "\n", &save);
    /*char key[256], value[256];
    sscanf(line, "%255[^=]=%255[^\n]%*c", key, value);*/
    if(strncmp(key, "file_capacity", 13) == 0){
        if(isInteger(value, &(config_struct.file_capacity))!=0)
            return EXIT_FAILURE;
    }else if(strncmp(key, "eviction_policy", 15) == 0){
        if(isInteger(value, &(config_struct.eviction_policy))!=0)
            return EXIT_FAILURE;
    }else if(strncmp(key, "byte_capacity", 13) == 0){
        if(isInteger(value, &(config_struct.byte_capacity))!=0)
            return EXIT_FAILURE;
    }else if(strncmp(key, "worker_threads", 14) == 0){
        if(isInteger(value, &(config_struct.worker_threads))!=0)
            return EXIT_FAILURE;
    }else if(strncmp(key, "max_connections_buffer_size", 27) == 0){
        if(isInteger(value, &(config_struct.max_connections_buffer_size))!=0)
            return EXIT_FAILURE;
    }else if(strncmp(key, "socket_path", 11) == 0){
        config_struct.socket_path = strdup(value); /*needs to be freed*/
    }else
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

/**
 * Init config structure
 */
void initConfigStruct(){
    config_struct.byte_capacity = -1;
    config_struct.file_capacity = -1;
    config_struct.max_connections_buffer_size = -1;
    config_struct.socket_path = NULL;
    config_struct.worker_threads = -1;
    config_struct.eviction_policy = -1;
}

/**
 * Check the configuration values
 * @return
 *      EXIT_FAILURE if any value is wrong
 *      EXIT_SUCCESS otherwise
 */
int checkConfigStructValues(){
    if( config_struct.byte_capacity <= 0 || config_struct.file_capacity <= 0 || 
            config_struct.max_connections_buffer_size <= 0 ||
            config_struct.socket_path == NULL || config_struct.worker_threads <= 0 ||
            config_struct.eviction_policy < 0 || config_struct.eviction_policy > 1 )
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

/**
 * Parse the config file and populates the config struct
 * @param configFilePath: the config file path
 * @return 
 *      EXIT_SUCCESS if everything's ok, 
 *      EXIT_FAILURE if something goes wrong
 */
char parseConfigFile(char* configFilePath){
    int bufSize = 256;
    char returnVal = EXIT_SUCCESS;

    FILE* fdi = fopen(configFilePath, "r");
    ec(fdi, NULL, return EXIT_FAILURE);

    char *buffer;
    ec(buffer = calloc(bufSize, sizeof(char)), NULL, return fd_cleanup(fdi));
    
    initConfigStruct();

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
    
    fd_cleanup(fdi);
    free(buffer);
    if( checkConfigStructValues() == EXIT_FAILURE )
        return EXIT_FAILURE;
    return returnVal;
}

/*--------- Signal handling part---------------------------------------------------*/
/**
 * Initialize sigmask for the server process
 * @return the new set
 */
sigset_t* initSigMask(){
    // Note that SIGPIPE is thrown when we try to write to a closed fd
    struct sigaction saa;
    memset(&saa, 0, sizeof(saa));
    // we will ignore SIGPIPE
    saa.sa_handler = SIG_IGN;
    ec( sigaction(SIGPIPE, &saa, NULL), -1, return NULL );

    sigset_t* set = NULL;
    ec( set=malloc(sizeof(sigset_t)), NULL, return NULL );
    /*we want to handle these signals, we will do it with sigwait*/
    ec( sigemptyset(set), -1, free(set); return NULL ); /*empty mask*/
    ec( sigaddset(set, SIGINT), -1, free(set); return NULL ); /* it will be handled with sigwait only */
    ec( sigaddset(set, SIGQUIT), -1, free(set); return NULL ); /* it will be handled with sigwait only */
    ec( sigaddset(set, SIGTSTP), -1, free(set); return NULL ); /* it will be handled with sigwait only */
    ec( sigaddset(set, SIGHUP), -1, free(set); return NULL ); /* it will be handled with sigwait only */
    
    ec( pthread_sigmask(SIG_SETMASK, set, &procOldMask), -1, free(set); return NULL );
    return set;
}

/**
 * Restore the original mask of the process
 */
void restoreOldMask(){
    ec( pthread_sigmask(SIG_SETMASK, &procOldMask, NULL), -1, );
}