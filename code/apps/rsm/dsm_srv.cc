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

#include "dsm.h"
#include <assert.h>
#include <netinet/tcp.h>

extern "C" void dsm_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);

#define BUFSZ (4096 * 16)

int main () {
    SVCXPRT *trans = svctcp_create(RPC_ANYSOCK, BUFSZ, BUFSZ);
    assert(trans);
    int on = 1;
    assert(0 == setsockopt(trans->xp_sock, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)));

    if (!svc_register(trans, DSM_PROG, DSM_VERS1, dsm_prog_1, 0)) {
        perror("svc_register");
        printf("oops registering svc!\n");
    }
    printf("Listening on port %d\n", trans->xp_port);
    fflush(stdout);
    svc_run();
    return -1;
}
