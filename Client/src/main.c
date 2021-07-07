#include<general_utility.h>
#include<cmdLineParser.h>
#include<client_API.h>
#include<conn.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char **argv){
    if(argc<=1){
        print_usage(argv[0]);
        return -1;
    }
    
    /*get list of operations to perform*/
    mainList_t ml;
    initML(&ml);
    if(parseCmdLine(argc, argv, &ml) == EXIT_FAILURE){
        freeList(ml);
        return EXIT_FAILURE;
    }

    /*check if socket path was specified, -D and -w/-W dependency and -d and -r/-R dependency*/
    if(ml.f_socketpath==NULL || (ml.D_dirname!=NULL && ml.w_dirname==NULL && ml.W_filenames == NULL) 
            ||(ml.d_dirname!=NULL && ml.r_filenames==NULL && ml.R_n==-1)){
        freeList(ml);
        return EXIT_FAILURE;
    }

    /*perform operations through API*/
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    spec.tv_sec += 5; /*try for 5 seconds at least*/
    /*first operation*/
    openConnection(ml.f_socketpath, ml.t_time, spec);
    
    ec_n( openFile("/file/path(14)", O_CREATE), 0, );
    ec_n( openFile("/file/path2(15)", O_CREATE|O_LOCK), 0, );
    //ec_n( lockFile("/file/path(14)"), 0, );
    //ec_n( unlockFile("/file/path(14)"), 0, );
    //ec_n( removeFile("/file/path(14)"), 0, );

    closeConnection(ml.f_socketpath);
    
    freeList(ml);
    return 0;
}
/*
int connetteScriveChiude(){
    int sockfd;
    ec( sockfd = socket(AF_UNIX, SOCK_STREAM, 0), -1, return EXIT_FAILURE );

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKPATH, strlen(SOCKPATH)+1);

    errno = 0;
    ec( connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), -1, return EXIT_FAILURE);
    
    int i = 3;
    while(i--){
        sendStringTo(sockfd, "Messaggio di 25 caratteri");

        char *response = NULL;
        getServerMessage(sockfd, &response);
        if(response!=NULL) free(response);
    }
    
    close(sockfd);
    return 0;
}*/