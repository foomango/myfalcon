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
#include "os_enforcer/os_worker.h"

#include <signal.h>
#include <libvirt/libvirt.h>

#include "common.h"

// How to config file this? Maybe pass it in from parent?
static const uint32_t kMemoryPollPeriod_ms = 100;
char* name;

inline static void
put_status(worker_state state) {
    if (sizeof(state) != write(fileno(stdout), &state, sizeof(state)))
        exit(EXIT_SUCCESS);
}

// If you cannot solve your problem, solve around your problem.
void
shutdown_notification_callback(int signum) {
    CHECK(signum == SIGUSR1);
    LOG("Got signal!");
    virConnectPtr conn = virConnectOpen(kHypervisorPath);
    virDomainPtr dom = virDomainLookupByName(conn, name);
    if (!dom) {
        put_status(VM_ERROR);
        exit(EXIT_FAILURE);
    }
    int ret = virDomainIsActive(dom);
    LOG("got shut down notification: %d", ret);
    if (ret == 1) {
        return;
    } else if (ret == 0) {
        put_status(VM_DEAD);
    } else {
        put_status(VM_NEEDS_KILL);
    }
    exit(EXIT_SUCCESS);
}

int
main(int argc, char** argv) {
    virDomainPtr dom = NULL;
    signal(SIGUSR1, shutdown_notification_callback);
    virConnectPtr conn = virConnectOpen(kHypervisorPath);
    CHECK(conn);
    name = argv[1];
    bool lethal = (argc == 3);
    dom = virDomainLookupByName(conn, name);
    if (!dom) {
        put_status(VM_ERROR);
        return EXIT_FAILURE;
    }
    for (;;) {
        int ret = virDomainDoNtfaProbe(dom, kMemoryPollPeriod_ms);
        switch (ret) {
            case NTFA_ALIVE:
                put_status(VM_OK);
                break;
            case NTFA_INACTIVE:
                put_status(VM_DEAD);
                return EXIT_SUCCESS;
            case NTFA_DEAD:
                LOG("Got NTFA_DEAD, trying to kill if allowed.");
                if (lethal) {
                    signal(SIGUSR1, SIG_IGN);
                    while (virDomainIsActive(dom)) {
                        virDomainDestroy(dom);
                    }
                    put_status(VM_KILLED);
                } else {
                    put_status(VM_NEEDS_KILL);
                }
                return EXIT_SUCCESS;
            default:
                put_status(VM_ERROR);
                return EXIT_FAILURE;
        }
    }
}
