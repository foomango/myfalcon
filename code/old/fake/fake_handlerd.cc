#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>

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
    serv_addr.sin_port = htons(kHandlerdPortNum);
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
        bzero(buffer,256);
        //n = read(newsockfd,buffer,255);
        n = recv(newsockfd, buffer, 255, 0);
        if (n < 0) error("ERROR reading from socket");
        printf("Here is the message: %s\n",buffer);
        handlerd_query *query = (handlerd_query *)buffer;
        struct handlerd_query_response reply;
        strcpy(reply.handle, "fake_handle");
        app_state_t state = DEAD;
        reply.state = state2char[state];
        reply.last_generation = 0x233;
        n = send(newsockfd, &reply, sizeof(reply), 0);
        //n = write(newsockfd,"I got your message",18);
        if (n < 0) error("ERROR writing to socket");
    }
}
