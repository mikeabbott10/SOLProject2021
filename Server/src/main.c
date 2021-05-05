#include<stdio.h>
#include<stdlib.h>
#include<server_config.h>
#include<general_utility.h>

int main(int args, char** argv){
    ec(parseConfigFile("./src/config.txt"), EXIT_FAILURE, , return 1); /*Please remember that using run.sh, current relative path is src/.. */
    /*printf("fileCapacity: %d\nbyteCapacity: %d\nworkerThreads: %d\nsocketPath: %s\n", 
        config_struct.file_capacity, config_struct.byte_capacity, config_struct.worker_threads, config_struct.socket_path);*/
    
    free(config_struct.socket_path);
    return 0;
}