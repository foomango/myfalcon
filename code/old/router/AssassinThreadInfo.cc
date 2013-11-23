#include "router/AssassinThreadInfo.h" 

#include <arpa/inet.h> 
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#include "common/common.h"
#include "router/util.h"

AssassinThreadInfo::AssassinThreadInfo(connection_type conn_type) {
    conn_type_ = conn_type;
    hostname_ = NULL;
}

AssassinThreadInfo::~AssassinThreadInfo() {
    if (fd_ >= 0) {
        close(fd_);
    }
    free(hostname_);
}

void
AssassinThreadInfo::SetHostname(char* hostname) {
    hostname_ = hostname;
}

char*
AssassinThreadInfo::GetHostname() {
    return hostname_;
}

int
AssassinThreadInfo::GetFd() const {
    return fd_;
}

bool
AssassinThreadInfo::InitConnection(int listen_fd) {    
    socklen_t len = sizeof(remote_addr_);
    fd_ = accept(listen_fd, (struct sockaddr*)&remote_addr_, &len);
    if (fd_ < 0) {
        return false;
    }
    return true;
}

void
AssassinThreadInfo::InitConnectionInfo() {
    void* addr = &(remote_addr_.sin_addr);
    CHECK(NULL != inet_ntop(remote_addr_.sin_family, addr, ip_addr_, 
                            sizeof(ip_addr_)));
    port_ = GetPortFromIP(ip_addr_);
    return;
}

bool
AssassinThreadInfo::CheckHostnameSanity(char *hostname) {
    struct addrinfo hints;
    struct addrinfo* ret;
    memset(&hints, '\0', sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(0 != getaddrinfo(hostname, NULL, &hints, &ret)) {
        return false;
    }
    char app_ip[INET_ADDRSTRLEN];
    memset(app_ip, '\0', sizeof(app_ip));

    struct sockaddr_in* ipv = (struct sockaddr_in*)ret->ai_addr;
    void* addr = &(ipv->sin_addr);

    if(NULL == inet_ntop(ret->ai_family, addr, app_ip, sizeof(app_ip))) {
        freeaddrinfo(ret);
        return false;
    }
    freeaddrinfo(ret);
    if(0 != strcmp(app_ip, ip_addr_)) {
        return false;
    }
    if (port_ < 0) {
        return false;
    }
    if(port_ != GetPort(hostname)) {
        IncrementGeneration(hostname);
        SetPort(hostname, port_);
    } 
    return true;
}
