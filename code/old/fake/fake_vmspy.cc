#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "common/common.h"
#include "common/messages.h"

int
main(int argc, char **argv) {
    /*
    int sockfd, newsockfd;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sockfd >= 0);

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(kVMSpyPort);
    inet_pton(AF_INET, "0.0.0.0", &(serv_addr.sin_addr));

    CHECK(0 == bind(sockfd, (struct sockaddr *) &serv_addr, 
                    sizeof(serv_addr)));
    
    listen(sockfd,10);
    socklen_t clilen = sizeof(cli_addr);

    while(true) {
        newsockfd = accept(sockfd, 
                (struct sockaddr *) &cli_addr, 
                &clilen);
        CHECK(newsockfd > 0);
        bzero(buffer,256);
        //n = read(newsockfd,buffer,255);
        n = recv(newsockfd, buffer, 255, 0);
        CHECK(n > 0);
        printf("Here is the message: %s\n",buffer);
        struct vmspy_response reply;
        app_state_t state = DEAD; 
        reply.status = state2char[state];
        n = send(newsockfd, &reply, sizeof(reply), 0);
        //n = write(newsockfd,"I got your message",18);
        CHECK(n > 0);
    }
    */
    return 0;
}
