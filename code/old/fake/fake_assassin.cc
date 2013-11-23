#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "common/common.h"
#include "common/messages.h"

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int
main(int argc, char **argv) {
    int sockfd, newsockfd;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(2000);
    inet_pton(AF_INET, "0.0.0.0", &(serv_addr.sin_addr));

    if (bind(sockfd, (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)) < 0) 
        error("ERROR on binding");
    listen(sockfd,10);
    socklen_t clilen = sizeof(cli_addr);


    while(true) {
        newsockfd = accept(sockfd, 
                (struct sockaddr *) &cli_addr, 
                &clilen);
        if (newsockfd < 0) 
            error("ERROR on accept");
    }
}
