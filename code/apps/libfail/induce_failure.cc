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
#include "client/client.h"
#include "saboteur.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <map>
#include <string>

using namespace Falcon;
typedef void(*FailFunc)();
uint16_t kServerFailPort = 12345;
uint32_t max_sleep_us;
LayerIdList layer_ids;

int
Connect(const std::string h, uint16_t port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    const char* hostname = h.c_str();
    CHECK(sock >= 0);
    struct sockaddr_in addr;
    struct hostent *ret = gethostbyname(hostname);
    struct in_addr* in_addr = (struct in_addr *) ret->h_addr_list[0];
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&(addr.sin_addr), in_addr, sizeof(struct in_addr));
    socklen_t len = sizeof(addr);
    connect(sock, (struct sockaddr *) &addr, len);
    return sock;
}

void
FailLivelock() {
    int sock = Connect(layer_ids[1], kProcessFailPort);
    const char* fail_string = "livelock";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
    return;
}

void
FailSegfault() {
    int sock = Connect(layer_ids[1], kProcessFailPort);
    const char* fail_string = "segfault";
    LOG("sending to %s %s on port %d", layer_ids[1].c_str(), fail_string, kProcessFailPort);
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
    return;
}

void
HangSpy() {
    int sock = Connect(layer_ids[1], kProcessFailPort);
    const char* fail_string = "spy_hang";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
    return;
}

void
InsideInfo() {
    int sock = Connect(layer_ids[1], kProcessFailPort);
    const char* fail_string = "use_ii";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
    return;
}

void
FailLoop() {
    int sock = Connect(layer_ids[1], kProcessFailPort);
    const char* fail_string = "badloop";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
    return;
}

void
FailHandlerdQuery(const char *word) {
    int sock = Connect(layer_ids[1], kServerFailPort);
    int len = strlen(word);
    CHECK(len == send(sock, word, len, 0));
    return;
}

void
FailHandlerd() {
    FailHandlerdQuery("fail_handlerd");
}

void
FailOS_OVERFLOW() {
    FailHandlerdQuery("fail_overflow");
}
void
FailOS_LOOP() {
    FailHandlerdQuery("fail_loop");
}
void
FailOS_PANIC() {
    FailHandlerdQuery("fail_panic");
}

void
FailOsEnforcer() {
    int sock = Connect(layer_ids[2], kServerFailPort);
    const char* fail_string = "os_enforcer";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
}

void
FailOsWorker() {
    int sock = Connect(layer_ids[2], kServerFailPort);
    const char* fail_string = "os_worker";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
}

void
FailIncrementer() {
  FailHandlerdQuery("incrementer");
}

void
FailVMMObserver() {
    int sock = Connect(layer_ids[2], kServerFailPort);
    const char* fail_string = "vmm_observer";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
}

void
FailLibvirtd() {
    int sock = Connect(layer_ids[2], kServerFailPort);
    const char* fail_string = "libvirtd";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
}

void
FailHypervisor() {
    int sock = Connect(layer_ids[2], kServerFailPort);
    const char* fail_string = "hypervisor";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
}

void
FailQemu() {
    int sock = Connect(layer_ids[2], kServerFailPort);
    const char* fail_string = "qemu";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
}

std::map<std::string,FailFunc>
InitFFMap() {
    std::map <std::string,FailFunc> m;
    m["segfault"] = FailSegfault;
    m["livelock"] = FailLivelock;
    m["badloop"] = FailLoop;
    m["hang_spy"] = HangSpy;
    m["use_ii"] = InsideInfo;
    m["handlerd"] = FailHandlerd;
    // Using the lkdtm module
    m["os_panic"] = FailOS_PANIC;
    m["os_overflow"] = FailOS_OVERFLOW;
    m["os_loop"] = FailOS_LOOP;
    // Using fault_server.py
    m["libvirtd"] = FailLibvirtd;
    m["hypervisor"] = FailHypervisor;
    m["qemu"] = FailQemu;
    // new failures
    m["os_enforcer"] = FailOsEnforcer;
    m["os_worker"] = FailOsWorker;
    m["incrementer"] = FailIncrementer;
    m["process_enforcer"] = FailHandlerd;
    m["vmm_observer"] = FailVMMObserver;
    return m;
}

std::map<std::string,FailFunc> failureFunctionMap = InitFFMap();

void
InduceFailure(std::string &failure) {
    if (failureFunctionMap[failure] == NULL) {
        std::fprintf(stderr, "Invalid failure type %s\n", failure.c_str());
        std::fprintf(stderr, "Valid options:\n");
        std::map<std::string,FailFunc>::iterator it;
        for (it = failureFunctionMap.begin();
             it != failureFunctionMap.end(); 
             it++) {
            std::fprintf(stderr, "\t%s\n", it->first.c_str());
        }
        exit(1);
    }
    failureFunctionMap[failure]();
    return;
}

void
GetApplicationId() {
    std::string app_handle, host_name, vmm_name, switch_name;
    CHECK(Config::GetFromConfig("app_handle", &app_handle));
    CHECK(Config::GetFromConfig("host_name", &host_name));
    CHECK(Config::GetFromConfig("vmm_name", &vmm_name));
    CHECK(Config::GetFromConfig("switch_name", &switch_name));
    layer_ids.push_back(app_handle);
    layer_ids.push_back(host_name);
    layer_ids.push_back(vmm_name);
    layer_ids.push_back(switch_name);
    return;
}

uint32_t
GetRandomSleep() {
    time_t t = time(NULL);
    srand(t);
    uint32_t ret = rand() % max_sleep_us;
    LOG("sleeping for %d us", ret);
    return ret;
}

double detected = -1.0;
pthread_mutex_t induce_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t induce_cond_ = PTHREAD_COND_INITIALIZER;

void
induce_cb(const LayerIdList& lids, void* NotUsed, uint32_t fs, uint32_t rs) {
    pthread_mutex_lock(&induce_lock_);
    detected = GetRealTime();
    LogCallbackData(lids, fs, rs);
    pthread_cond_signal(&induce_cond_);
    pthread_mutex_unlock(&induce_lock_);
}

int
main (int argc, char **argv) {
    Config::LoadConfig(argv[1]);
    bool get_status, lethal;
    int e2etimeout;
    std::string failure;
    Config::GetFromConfig("get_status", &get_status, true);
    Config::GetFromConfig("max_sleep_us", &max_sleep_us,
                          (uint32_t)(100 * kMillisecondsToMicroseconds));
    Config::GetFromConfig("lethal", &lethal, false);
    CHECK(Config::GetFromConfig("failure", &failure));
    Config::GetFromConfig("e2etimeout", &e2etimeout, 300);
    GetApplicationId();
    falcon_target* t = NULL;
    if (get_status) {
        t = init(layer_ids, lethal, NULL);
        startMonitoring(t, induce_cb, e2etimeout);
    }
    LOG1("got app instance");
    if (get_status) {
        sleep(4);
        usleep(GetRandomSleep()); // This sleep is to be random
    }
    LOG1("inducing failure");
    double strt = GetRealTime();
    InduceFailure(failure);
    double induced = GetRealTime();
    pthread_mutex_lock(&induce_lock_);
    if (get_status) {
        while (detected < 0) {
            pthread_cond_wait(&induce_cond_, &induce_lock_);
        }
        std::printf("%f\t%f\n", induced - strt, detected - induced);
        std::printf("%f\t%f\n", strt, induced);
    } else {
        std::printf("%f\t%f\n", strt, induced);
    }
    pthread_mutex_unlock(&induce_lock_);
    LOG("sleeping to allow cancellation");
    sleep(10);
    LOG("done sleeping to allow cancellation");
    return 0;
}
