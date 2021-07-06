#if !defined(_SERVER_CONFIG_H)
#define _SERVER_CONFIG_H
#define _POSIX_C_SOURCE 200809L
#include<signal.h>
#include<pthread.h>

int progress_level = 0; /*useful for freeing the allocated memory only*/

/* 
    Server configurations are stored in this struct.
    If it will be edited, please get server_config.c edited too.
*/
typedef struct{
    int file_capacity; /*server file capacity*/
    int byte_capacity; /*server byte capacity*/
    int worker_threads; /*number of workers*/
    int max_connections_buffer_size; /*connections buffer capacity*/
    int eviction_policy; /*the eviction policy*/
    char* socket_path;
} server_config_t;

server_config_t config_struct;

pthread_t sig_handler_tid;
sigset_t procOldMask;
sigset_t procCurrentMask;

char parseConfigFile(char*); 
sigset_t initSigMask();
void restoreOldMask();

#endif