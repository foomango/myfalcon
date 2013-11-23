#ifndef _NTFA_ROUTER_CONNECTING_THREAD_INFO_
#define _NTFA_ROUTER_CONNECTING_THREAD_INFO_

#include <netinet/in.h>
#include <pthread.h>

#include "router/util.h"

typedef enum {
    AGENT,
    CLIENT
} connection_type;

class AssassinThreadInfo {
    public:
        AssassinThreadInfo(connection_type conn_type);
        ~AssassinThreadInfo();
        int GetFd() const;
        void SetHostname(char* hostname);
        char* GetHostname();
        bool InitConnection(int listen_fd);
        void InitConnectionInfo();
        bool CheckHostnameSanity(char* hostname);

    private:
        connection_type conn_type_;
        int fd_;
        struct sockaddr_in remote_addr_;
#define IP_BUF_SIZE 32
        char ip_addr_[IP_BUF_SIZE];
        int port_;
        char* hostname_;
};
#endif //_NTFA_PFD_CONNECTING_THREAD_INFO_
