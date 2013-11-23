#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common/common.h"
#include "common/messages.h"

const int kBufferSize = 256;
const char* kRouterAddr = "192.168.1.1";

int
ThreadsafeConnect(const char *hostname, const uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sock >= 0);
    struct addrinfo hints;
    struct addrinfo* ret;
    memset(&hints, '\0', sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_num[kBufferSize];
    sprintf(port_num, "%9d", port); 
    if(0 != getaddrinfo(hostname, port_num, &hints, &ret)) {
        return -1;
    }
    CHECK(ret->ai_family == AF_INET);
    if (0 != connect(sock, ret->ai_addr, ret->ai_addrlen)) {
        LOG1("failed to connect to host");
        freeaddrinfo(ret);
        return -1;
    }
    freeaddrinfo(ret);
    int on = 1;
    CHECK(0 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)));
    return sock;
}

int 
main(int argc, char* argv[]) {
    if(argc < 4) {
        fprintf(stderr, "input error, ./mock_client libvirtd_name gen_num time_out_ms\n");
        exit(EXIT_FAILURE);
    }

    int switch_sock = ThreadsafeConnect(kRouterAddr, kQueryPort);
    CHECK(switch_sock >= 0);

    struct router_query query;
    strcpy(query.hostname, argv[1]);
    printf("query hostname: %s\n", query.hostname);
    query.generation_num = htonl(atol(argv[2]));
    printf("query generation_num: %d\n", ntohl(query.generation_num));
    query.timeout_ms = htonl(atoi(argv[3]));
    printf("query timeout_ms: %d\n", ntohl(query.timeout_ms));

    int ret_val = send(switch_sock, &query, sizeof(query), 0);

    if (ret_val != sizeof(query)) {
    	LOG1("Error sending assassination request");
        close(switch_sock);
        exit(EXIT_FAILURE);
    }
    printf("send succ\n");

    struct router_response resp;
    ret_val = recv(switch_sock, &resp, sizeof(resp), 0);
    close(switch_sock);
    if (ret_val < 0) {
    	LOG1("Error receiving assassination response");
        close(switch_sock);
        exit(EXIT_FAILURE);
    }
    if (resp.state == 'D') {
    	LOG1("libvirtd dead");
        close(switch_sock);
    } else if (resp.state == 'A') {
    	LOG1("libvirtd alive");
        close(switch_sock);
    } else if (resp.state == 'I') {
    	LOG1("libvirtd unknow");
        close(switch_sock);
    }
    printf("recv succ\n");
    return 0;
}


