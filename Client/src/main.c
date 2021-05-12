#include<general_utility.h>
#include<conn.h>
#include<stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKPATH "../LSOfiletorage.sk" // TODO: not here

int main(){
    int sockfd;
    ec( sockfd = socket(AF_UNIX, SOCK_STREAM, 0), -1, return EXIT_FAILURE );

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKPATH, strlen(SOCKPATH)+1);

    errno = 0;
    ec( connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), -1, return EXIT_FAILURE);
    
    sendStringTo(sockfd, "Messaggio di 25 caratteri");

    char *response = NULL;
    getServerMessage(sockfd, &response);
    if(response!=NULL) free(response);
    
    close(sockfd);
    return 0;
}