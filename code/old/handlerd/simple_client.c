#include "handlerd/handlerd.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int
main() {
    struct sockaddr_in net_addr;
    memset(&net_addr, '\0', sizeof(net_addr));
    net_addr.sin_family = AF_INET;
    net_addr.sin_port = htons(kPortNum);
    // Localhost for now...
    inet_pton(AF_INET, "127.0.0.1", &(net_addr.sin_addr));
    int my_sock;
    struct ext_query out;
    struct reply my_reply;
    strcpy(out.dst_handle, "ntfa_loler");
    strcpy(out.src_handle, "ntfa_test");
    bool broke = false;
    while (!broke) {
            printf(".");
            out.timeout_ms = 20;
            my_sock = socket(AF_INET, SOCK_STREAM, 0);
            out.timeout_ms = htonl(out.timeout_ms);
            assert(0 == connect(my_sock, (struct sockaddr *) &net_addr, sizeof(net_addr)));
            assert(sizeof(out) == send(my_sock, &out, sizeof(out), 0));
            assert(sizeof(my_reply) == recv(my_sock, &my_reply, sizeof(my_reply), 0 ));
            close(my_sock);
            out.timeout_ms = ntohl(out.timeout_ms);
            if (my_reply.isDead) {
                printf("%d\n", out.timeout_ms);
                broke = true;
            }
            fflush(stdout);
            usleep(1000);
    }
    printf("\n");
    return 0;
}

