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

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/clnt.h>
#include <time.h>

#include <string>
#include <list>
#include <iostream>
#include <sstream>

#include "dsm.h"

#include "common.h"

const char *memory_log = "/dev/shm/client.log";

CLIENT*
ThreadsafeConnect(const char *hostname, const uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    struct addrinfo hints;
    struct addrinfo* ret;
    memset(&hints, '\0', sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(0 != getaddrinfo(hostname, NULL, &hints, &ret)) {
        return NULL;
    }
    reinterpret_cast<struct sockaddr_in *>(ret->ai_addr)->sin_port = htons(port);
    assert(ret->ai_family == AF_INET);
    if (0 != connect(sock, ret->ai_addr, ret->ai_addrlen)) {
        printf("failed to connect to host %s:%u", hostname, port);
        freeaddrinfo(ret);
        return NULL;
    }
    CLIENT* clnt = clnttcp_create((sockaddr_in *)ret->ai_addr, DSM_PROG, DSM_VERS1, &sock, 0, 0);
    freeaddrinfo(ret);
    if (!clnt) {
        clnt_pcreateerror("any");
        return NULL;
    }
    int on = 1;
    assert(0 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)));
    assert(clnt_control(clnt, CLSET_FD_CLOSE, (char *) &on));
    return clnt;
}

int
main(int argc, char **argv) {
    // This doesn't use config infrastructure, but oh well.
    const char *hostname = argv[1];
    uint16_t port = atoi(argv[2]);
    uint32_t count = atoi(argv[3]);
    // Client vm shouldn't ever crash. Here is where we'll
    // get the logs from
    freopen(memory_log, "w+", stderr);
    CLIENT *clnt;
    clnt = ThreadsafeConnect(hostname, port);
    assert(clnt);
    enum clnt_stat st;
    dsm_count res;

    int err_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        LOG1("start request");
        int xid;
        do {
            st = dsm_inc_1(0, &res, clnt);
            if (st != RPC_SUCCESS) {
                LOG("Status %d", st);
                // Added to break out of weird failures
                err_count++;
                if (err_count > 10) exit(1);
            }
        } while (st != RPC_SUCCESS);
        LOG1("end request");
    }
    return 0;
}
