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
#include "client/FalconClient.h"
#include "process_enforcer/process_observer.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

int64_t last_check = 0;
int64_t counter = 0;

uint32_t
MySpy() {
    uint32_t ret = (last_check != counter) ? PROC_OBS_ALIVE : PROC_OBS_DEAD;
    last_check = counter;
    return ret;
}

pthread_mutex_t lockA = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;

const char *my_handle = "ntfa_deadlock";
const int32_t kTimeToSleepUsec = 1;

void *
Thread(void *_args) {
    pthread_detach(pthread_self());
    for (;;) {
        pthread_mutex_lock(&lockA);
        pthread_mutex_lock(&lock1);
        counter++;
        pthread_mutex_unlock(&lock1);
        pthread_mutex_unlock(&lockA);
        usleep(kTimeToSleepUsec);
    }
    return NULL;
}

int
main () {
    SetHandle(my_handle);
    SetSpy(&MySpy, my_handle);
    pthread_t _unused;
    pthread_create(&_unused, NULL, &Thread, NULL);
    for(;;) {
        pthread_mutex_lock(&lock1);
        pthread_mutex_lock(&lockA);
        counter++;
        pthread_mutex_unlock(&lockA);
        pthread_mutex_unlock(&lock1);
        usleep(kTimeToSleepUsec);
    }
    return 0;
}
