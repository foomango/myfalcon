/* 
 * Copyright (c) 2011 Joshua B. Leners (University of Texas at Austin).
 * All rights reserved.
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of Texas at Austin. The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 */

#include "diomedes/diomedes_proxy.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <map>

#include "common.h"
#include "diomedes/diomedes.h"
#include "diomedes/diomedes_message.h"

namespace Diomedes {

// File descriptors
int client_listen_socket_;
int diomedes_socket_;
int my_epoll_;
uint32_t my_id_;
uint16_t next_client_id_ = 0;

sockaddr_in bcast_addr_;

std::map<client_info,int> client_info_to_client_;
std::map<client_info,uint32_t> client_info_to_client_id_;
std::map<uint32_t,client_info> client_id_to_client_info_;

void
AcceptNewClient() {
    sockaddr_in peer;
    socklen_t alen = sizeof(peer);
    int new_fd = accept(client_listen_socket_, (sockaddr *) &peer, &alen);
    CHECK(new_fd >= 0);
    client_info info(ntohl(peer.sin_addr.s_addr), ntohs(peer.sin_port));
    client_info_to_client_[info] = new_fd;
    uint32_t id = my_id_ | next_client_id_;
    next_client_id_++;
    client_info_to_client_id_[info] = id;
    client_id_to_client_info_[id] = info;
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = new_fd;
    ev.events = EPOLLIN;
    CHECK(0 == epoll_ctl(my_epoll_, EPOLL_CTL_ADD, new_fd, &ev));
    return;
}

void
ProcessClientRequest(int client) {
    static char buf[kMaxDiomedesMessage];
    memset(buf, 0, kMaxDiomedesMessage);
    diomedes_message* msg = (diomedes_message *) buf;
    sockaddr_in peer;
    socklen_t sock_size = sizeof(peer);
    if (0 != getpeername(client, (sockaddr*) &peer, &sock_size)) {
        close(client);
        return;
    }
    client_info info(ntohl(peer.sin_addr.s_addr), ntohs(peer.sin_port));
    // Build message to diomedes server
    msg->type = CLIENT_REQUEST;
    msg->version = 0;
    uint32_t max_msg_size = kMaxDiomedesMessage - kDiomedesHeaderSize
                            - (2 * sizeof(uint32_t));
    uint16_t recv_size = recv(client, &(msg->payload.creq.request), max_msg_size, 0);
    if (recv_size <= 0) {
        close(client);
        client_info_to_client_[info] = -1;
        return;
    }
    msg->length = htons((2 * sizeof(uint32_t) + recv_size));
    msg->payload.creq.client_id = htonl(client_info_to_client_id_[info]);
    int send_size = ntohs(msg->length) + kDiomedesHeaderSize;
    socklen_t alen = sizeof(bcast_addr_);
    CHECK(send_size == sendto(diomedes_socket_, msg, send_size, 0,
                              (sockaddr *) &bcast_addr_, alen));
    return;
}

void
ProcessDiomedesResponse() {
    static char buf[kMaxDiomedesMessage];
    memset(buf, 0, kMaxDiomedesMessage);
    diomedes_message* msg = (diomedes_message *) buf;
    sockaddr_in from;
    socklen_t alen = sizeof(from);
    // Don't care who it's from.
    int recv_len = recvfrom(diomedes_socket_, msg, kMaxDiomedesMessage, 0,
                            (sockaddr *) &from, &alen);
    CHECK(recv_len > 0);
    uint32_t id = ntohl(msg->payload.resp.client_id);
    uint32_t length = 0xFFFFFF & ntohl(msg->payload.resp.request.size);
    int send_size = length + sizeof(uint32_t);
    client_info info = client_id_to_client_info_[id];
    int client = client_info_to_client_[info];
    // Never close stdin
    if (client <= 0) {
        return;
    }
    if(send_size != send(client, &(msg->payload.resp.request), send_size, 0)) {
        close(client);
    }
    return;
}

void
ProxyRun() {
    epoll_event events[kNumEpollEvents];
    for (;;) {
        int nevents = epoll_wait(my_epoll_, events, kNumEpollEvents, -1);
        for (int i = 0; i < nevents; i++) {
            int curr_fd = events[i].data.fd;
            if (curr_fd == diomedes_socket_) {
                    ProcessDiomedesResponse();
            } else if (curr_fd == client_listen_socket_) {
                    AcceptNewClient();
            } else {
                    ProcessClientRequest(curr_fd);
            }
        }
    }
    return;
}

void
ProxyInit() {
    // for setsockopt
    int on = 1;
    // bcast addr
    bcast_addr_.sin_family = AF_INET;
    bcast_addr_.sin_port = htons(kDiomedesClientPort);
    bcast_addr_.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // client listen    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDiomedesClientPort + 2);
    client_listen_socket_ = socket(PF_INET, SOCK_STREAM, 0);
    CHECK(client_listen_socket_ >= 0);
    CHECK(0 == bind(client_listen_socket_, (sockaddr *) &addr, sizeof(addr)));
    CHECK(0 == listen(client_listen_socket_, 5));

    // Diomedes socket
    diomedes_socket_ = socket(PF_INET, SOCK_DGRAM, 0);
    CHECK(0 == bind(diomedes_socket_, (sockaddr *) &addr, sizeof(addr)));
    CHECK(0 == setsockopt(diomedes_socket_, SOL_SOCKET, SO_BROADCAST, &on,
                          sizeof(on)));
    // Create epoll
    my_epoll_ = epoll_create(6); // arg is ignored
    CHECK(my_epoll_ >= 0);
    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;

    ev.data.fd = client_listen_socket_;
    CHECK(0 == epoll_ctl(my_epoll_, EPOLL_CTL_ADD, client_listen_socket_, &ev));
    ev.data.fd = diomedes_socket_;
    CHECK(0 == epoll_ctl(my_epoll_, EPOLL_CTL_ADD, diomedes_socket_, &ev));
    // TODO(leners): Get config options
    return;
}

}

int
main() {
    Diomedes::ProxyInit();
    Diomedes::ProxyRun();
    return 1;
}
