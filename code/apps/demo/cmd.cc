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
#include "config.h"
#include "client/FalconClient.h"
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

pthread_mutex_t wait_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_cond_ = PTHREAD_COND_INITIALIZER;
bool called_back_ = false;

QualifiedHandle
GetApplicationId(const char *filename) {
    std::string app_handle, host_name, vmm_name, switch_name;
    QualifiedHandle qh;
    Config::LoadConfig(filename);
    CHECK(Config::GetFromConfig("app_handle", &app_handle));
    CHECK(Config::GetFromConfig("host_name", &host_name));
    CHECK(Config::GetFromConfig("vmm_name", &vmm_name));
    CHECK(Config::GetFromConfig("switch_name", &switch_name));
    qh.push_back(app_handle);
    qh.push_back(host_name);
    qh.push_back(vmm_name);
    qh.push_back(switch_name);
    return qh;
}

void
cb(const QualifiedHandle& h, uint32_t fs, uint32_t rs) {
    LOG("got response %d %d", fs, rs);
    called_back_ = true;
    pthread_mutex_lock(&wait_lock_);
    pthread_cond_signal(&wait_cond_);
    pthread_mutex_unlock(&wait_lock_);
}

int
main (int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s config_file", argv[0]);
    }
    FalconClient *cl = FalconClient::GetInstance();
    QualifiedHandle qh = GetApplicationId(argv[1]);
    bool lethal;
    Config::GetFromConfig("lethal", &lethal, true);
    cl->StartMonitoring(qh, lethal, cb);
    pthread_mutex_lock(&wait_lock_);
    while (!called_back_) {
        pthread_cond_wait(&wait_cond_, &wait_lock_);
    }
    pthread_mutex_unlock(&wait_lock_);
    return 0;
}
