#if !defined(_SERVER_CONFIG_H)
#define _SERVER_CONFIG_H

/* Server configurations are stored in this struct */
typedef struct{
    int file_capacity;
    int byte_capacity;
    int worker_threads;
    char* socket_path;
} Config;

Config config_struct;

char parseConfigFile(char*); 

#endif