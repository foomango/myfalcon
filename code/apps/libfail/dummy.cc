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

#include "common.h"
#include "saboteur.h"
#include "process_enforcer/process_observer.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;

bool should_hang_ = false;
bool should_fail_ = false;

bool
ShouldFail() {
    bool ret;
    pthread_mutex_lock(&lock_);
    ret = should_fail_;
    pthread_mutex_unlock(&lock_);
    return ret;
}

bool
ShouldHang() {
    bool ret;
    pthread_mutex_lock(&lock_);
    ret = should_hang_;
    pthread_mutex_unlock(&lock_);
    return ret;
}

void
SetHang() {
    pthread_mutex_lock(&lock_);
    should_hang_ = true;
    pthread_mutex_unlock(&lock_);
    return;
}

void
SetFail() {
    pthread_mutex_lock(&lock_);
    should_fail_ = true;
    pthread_mutex_unlock(&lock_);
    return;
}


uint32_t
MySpy(void) {
    if (ShouldHang()) {
        for(;;);
    }
    return (ShouldFail()) ? PROC_OBS_DEAD : PROC_OBS_ALIVE;
}

const char *my_handle = "dummy";

void
Daemonize(const char *logfile) {
    pid_t pid, sid;
    if (getppid() == 1) return;
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    umask(0);
    sid = setsid();
    if (sid < 0) exit(EXIT_FAILURE);
    if (chdir("/") < 0) exit(EXIT_FAILURE);

    freopen ("/dev/null", "r", stdin);
    freopen (logfile, "a", stdout);
    freopen (logfile, "a", stderr);
    CHECK(0 == fcntl(0, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(1, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(2, F_SETFL, O_NONBLOCK));
}

void
Saboteur(const char *failure) {
    if (!strcmp(failure, "segfault")) {
        int *x = NULL;
        *x = 11;
    } else if  (!strcmp(failure, "spy_hang")) {
        SetHang();
    } else if (!strcmp(failure, "use_ii")) {
        SetFail();
    }
}

int
main (int argc, char **argv) {
    Daemonize(argv[1]);
    SetHandle(my_handle);
    SetSpy(&MySpy, my_handle);
    SetSabotage(&Saboteur);
    for (;;) {
        sleep(10);
    }
    return 0;
}
