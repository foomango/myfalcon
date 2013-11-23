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

#include "saboteur.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

namespace {
const uint32_t kFailBufferSize = 512;
sabotage_f Sabotage;

void *
Saboteur(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(sock >= 0);
    int on = 1;
    CHECK(0 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)));
    struct sockaddr_in addr;
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kProcessFailPort);
    inet_pton(AF_INET, "0.0.0.0", &(addr.sin_addr));
    CHECK(0 == bind(sock, (struct sockaddr *) &addr, sizeof(addr)));
    char buf[kFailBufferSize];
    CHECK(0 < recv(sock, buf, kFailBufferSize, 0));
    LOG("got failure %s flushing and crashing", buf);
    fflush(stderr);
    fsync(fileno(stderr));
    Sabotage(buf);
    fflush(stderr);
    fsync(fileno(stderr));
    return NULL;
}

}

void
SetSabotage(sabotage_f sabotage_func) {
    Sabotage = sabotage_func;
    pthread_t thread;
    CHECK(0 == pthread_create(&thread, NULL, Saboteur, NULL));
}
