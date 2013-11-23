#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <map>

#include "common/common.h"
#include "common/messages.h"
#include "router/AssassinThreadInfo.h"
#include "router/bridge/libbridge/libbridge.h"
#include "router/util.h"

static const int32_t kHostNameLen = 64;

bool
KillLibvirtd(char* hostname) {
    char path[512];
    int port = GetPort(hostname);
    sprintf(path, "/proc/switch/eth0/port/%d/enable", port);
    FILE *port_file = fopen(path, "w");
    if (port_file == NULL) return false;
    fputc('0', port_file);
    fputc('\n', port_file);
    fclose(port_file);
    return true;
}

int
CreateListenSocket(char* ip_addr, int port_num) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sock >= 0);
    struct sockaddr_in listen_addr;
    memset(&listen_addr, '\0', sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(port_num);
    // inet_pton is weird in that 1 == success
    CHECK(1 == inet_pton(AF_INET, ip_addr, &listen_addr.sin_addr)); 
    CHECK(0 == bind(sock, (struct sockaddr *) &listen_addr,
                sizeof(listen_addr)));
    CHECK(0 == listen(sock, 10));
    return sock;
}

void*
ClientHandler(void* arg) {
    AssassinThreadInfo* conn_info = reinterpret_cast<AssassinThreadInfo*>(arg);
    int fd = conn_info->GetFd();

    struct router_query q;
    int send_ret;
    int recv_ret = recv(fd, &q, sizeof(q), 0);
    if(recv_ret != sizeof(q)) {
        LOG1("error receiving query");
        delete(conn_info);
        return NULL;
    }
    char* hostname = q.hostname;
    struct router_response r;
    uint32_t gen_num = ntohl(q.generation_num);
    bool beakless = false;
    uint32_t beakmask = 1 << (sizeof(uint32_t) * 8 - 1);
    if (gen_num & beakmask) {
        beakless = true;
        gen_num ^= beakmask;
    }
    enum libvirt_state as = GetLibvirtState(hostname, q);
    LOG("Query: %s", hostname);
    if (as == ACTIVE) {
        as = ClientWaitResponse(hostname, gen_num);
    }
    char c;
    int alive = recv(fd, &c, 1, MSG_DONTWAIT);
    if (alive < 0) {
        switch (as) {
            case INVALID:
            case FUTURE:
                r.state = 'I';
                break;
            case TIMEDOUT:
                if (!beakless) {
                    KillLibvirtd(hostname);
                    LogGeneration(hostname);
                    IncrementGeneration(hostname);
                }
            case LONG_DEAD:
                r.state = 'D';
                break;
            default:
                r.state = 'A';
                break;
        }
        LOG("sending query response: %c", r.state);
        r.last_generation = htonl(GetGeneration(hostname));
        send_ret = send(fd, &r, sizeof(r), 0);
        if(send_ret != sizeof(r)) {
            LOG1("error sending query response");
        }
    } else {
        LOG1("Disconnected, not doing anything");
    }
    delete(conn_info);
    return NULL;
}

bool
AgentHandshake(AssassinThreadInfo* conn_info) {
    int fd = conn_info->GetFd();
    char* hostname = (char*) malloc(HOSTNAME_SIZE);
    memset(hostname, '\0', HOSTNAME_SIZE);
    conn_info->SetHostname(hostname);
    struct libvirtd_handshake libv_hs;
    int recv_ret = recv(fd, &libv_hs, sizeof(libv_hs), 0);
    // Reasons the handshake can fail:
    // (1) The connection fails
    if(recv_ret != sizeof(libv_hs)) {
        LOG1("receiving hostname error");
        return false;
    }
    strncpy(hostname, libv_hs.hostname, HOSTNAME_SIZE);
    // (2) The connection fails to pass sanity checks.
    conn_info->InitConnectionInfo();
    if(!conn_info->CheckHostnameSanity(hostname)) {
        LOG1("check hostname error");
        return false;
    }
    uint32_t gen_num = GetGeneration(hostname);
    // (3) Connection fails later
    LOG("host: %s gen: %u", hostname, gen_num);
    struct router_handshake router_hs; 
    router_hs.generation_num = htonl(gen_num);
    int send_ret = send(fd, &router_hs, sizeof(router_hs), 0);
    if(send_ret != sizeof(router_hs)) {
        LOG1("send generation # error");
        return false;
    }
    return true;
}

#ifndef ROUTER_POLL_US
#define ROUTER_POLL_US 100000
#endif

void*
AgentHandler(void* arg) {
    AssassinThreadInfo* conn_info = reinterpret_cast<AssassinThreadInfo*>(arg);
    if (!AgentHandshake(conn_info)) {
        delete conn_info;
        return NULL;
    }
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = ROUTER_POLL_US;
    int fd = conn_info->GetFd();
    char *hostname = conn_info->GetHostname();
    struct keepalive_msg msg;
    int send_ret, recv_ret;
    SetAgentStatus(hostname, true);
    for(;;) {
        send_ret = send(fd, &msg, sizeof(msg), 0);
        if (send_ret <= 0) {
            break;
        }
        usleep(ROUTER_POLL_US);
        recv_ret = recv(fd, &msg, sizeof(msg), MSG_DONTWAIT);
        if (recv_ret <= 0) {
            break;
        }
    }
    LOG("we lost %s", hostname);
    BroadcastAgentResponse(hostname);
    delete conn_info;
    return NULL;
}

void*
ListenForClient(void* arg) {
    int client_sock = CreateListenSocket(reinterpret_cast<char*>(arg), 
                                         kQueryPort);
    for(;;) {
        AssassinThreadInfo* conn_info = new AssassinThreadInfo(CLIENT);
        if(!conn_info->InitConnection(client_sock)) {
            delete conn_info;
            continue;
        }
        pthread_t new_client;
        pthread_create(&new_client, NULL, ClientHandler, (void*)conn_info);
        pthread_detach(new_client);
    }
    return NULL;
}

void*
ListenForAgent(void* arg) {
    int agent_sock = CreateListenSocket(reinterpret_cast<char*>(arg),
                                        kKeepAlivePort);
    for(;;) {
        AssassinThreadInfo* conn_info = new AssassinThreadInfo(AGENT);
        if(!conn_info->InitConnection(agent_sock)) {
            delete conn_info;
            continue;
        }
        pthread_t new_agent;
        pthread_create(&new_agent, NULL, AgentHandler, (void*)conn_info);
        pthread_detach(new_agent);
    }
    return NULL;
}

int
main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Please specify an IP to listen on\n");
        exit(EXIT_FAILURE);
    }

    InitGenerationLog(); 
    Daemonize();
    br_init();
    pthread_t agent_listener;
    pthread_t client_listener;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&agent_listener, &attr, ListenForAgent, (void*)argv[1]);
    pthread_create(&client_listener, &attr, ListenForClient, (void*)argv[1]);
    pthread_exit(NULL);
    return 0;
}
