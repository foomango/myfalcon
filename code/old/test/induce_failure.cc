#include "common/common.h"
#include "common/messages.h"
#include "common/config.h"
#include "pfd/pfd.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <map>
#include <string>

typedef void(*FailFunc)();
Application *app_id;
ApplicationInstance *app_instance;
uint16_t kFailPortNum = 12345;
uint32_t max_sleep_us;

int
Connect(const char* hostname, uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
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
    int sock = Connect(app_id->GetHostname(), kFailPortNum);
    const char* fail_string = "livelock";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
    return;
}

void
FailSegfault() {
    int sock = Connect(app_id->GetHostname(), kFailPortNum);
    const char* fail_string = "segfault";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
    return;
}

void
FailHandlerdQuery(const char *word) {
    int sock = Connect(app_id->GetHostname(), kHandlerdPortNum);
    struct handlerd_query failure_query;
    CopyHandle(failure_query.dst_handle, word);
    failure_query.app_generation = app_instance->GetApplicationGenerationNumber();
    failure_query.app_generation = app_instance->GetDomainGenerationNumber();
    failure_query.app_generation = app_instance->GetHypervisorGenerationNumber();
    CHECK(sizeof(failure_query) == send(sock, &failure_query, sizeof(failure_query), 0));
    return;
}

void
HangSpy() {
    int sock = Connect(app_id->GetHostname(), kFailPortNum);
    const char* fail_string = "spy_hang";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
    return;
}

void
InsideInfo() {
    int sock = Connect(app_id->GetHostname(), kFailPortNum);
    const char* fail_string = "use_ii";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
    return;
}


void
FailLoop() {
    int sock = Connect(app_id->GetHostname(), kFailPortNum);
    const char* fail_string = "badloop";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
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
FailLibvirtd() {
    int sock = Connect(app_id->GetVMMHostname(), kFailPortNum);
    const char* fail_string = "libvirtd";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
}

void
FailHypervisor() {
    int sock = Connect(app_id->GetVMMHostname(), kFailPortNum);
    const char* fail_string = "hypervisor";
    int len = strlen(fail_string);
    CHECK(len == send(sock, fail_string, len, 0));
}

void
FailQemu() {
    int sock = Connect(app_id->GetVMMHostname(), kFailPortNum);
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

Application*
GetApplicationId() {
    std::string app_handle, host_name, vmm_name, switch_name;
    long handlerd_delay;
    long vmspy_delay;
    long switch_delay;
    long handlerd_spy_timeout;
    long handlerd_confirm_timeout;
    long vmspy_timeout;
    long switch_timeout;
    CHECK(Config::GetFromConfig("app_handle", &app_handle));
    CHECK(Config::GetFromConfig("host_name", &host_name));
    CHECK(Config::GetFromConfig("vmm_name", &vmm_name));
    CHECK(Config::GetFromConfig("switch_name", &switch_name));
    Application *app =
        new Application(app_handle.c_str(),
                        host_name.c_str(),
                        vmm_name.c_str(),
                        switch_name.c_str());
    Config::GetFromConfig("handlerd_delay",
            &handlerd_delay, (long)kDefaultHandlerdStartDelay);
    Config::GetFromConfig("vmspy_delay",
            &vmspy_delay, (long)kDefaultVMSpyStartDelay);
    Config::GetFromConfig("switch_delay",
            &switch_delay, (long)kDefaultRouterSpyStartDelay);
    Config::GetFromConfig("handlerd_spy_timeout",
            &handlerd_spy_timeout, (long)kDefaultHandlerdSpyTimeout);
    Config::GetFromConfig("handlerd_confirm_timeout",
            &handlerd_confirm_timeout, (long)kDefaultHandlerdConfirmTimeout);
    Config::GetFromConfig("vmspy_timeout",
            &vmspy_timeout, (long)kDefaultVMSpyTimeout);
    Config::GetFromConfig("switch_timeout",
            &switch_timeout, (long)kDefaultRouterSpyTimeout);
    app->SetHandlerdStartDelay(handlerd_delay);
    app->SetVMSpyStartDelay(vmspy_delay);
    app->SetRouterSpyStartDelay(switch_delay);
    app->SetHandlerdSpyTimeout(handlerd_spy_timeout);
    app->SetHandlerdConfirmTimeout(handlerd_confirm_timeout);
    app->SetVMSpyTimeout(vmspy_timeout);
    app->SetRouterSpyTimeout(switch_timeout);
    return app;
}

uint32_t
GetRandomSleep() {
    time_t t = time(NULL);
    srand(t);
    uint32_t ret = rand() % max_sleep_us;
    LOG("sleeping for %d us", ret);
    return ret;
}


int
main (int argc, char **argv) {
    PFD *pfd = PFD::Instance();
    if (argc != 2) {
        std::printf("usage: %s config_file\n", argv[0]);
        exit(1);
    }
    Config::LoadConfig(argv[1]);
    bool get_status;
    std::string failure;
    Config::GetFromConfig("get_status", &get_status, true);
    Config::GetFromConfig("max_sleep_us", &max_sleep_us, (uint32_t)(kDefaultHandlerdSpyTimeout * kMillisecondsToMicroseconds));
    CHECK(Config::GetFromConfig("failure", &failure));
    app_id = GetApplicationId();
    LOG1("got app id");
    if (get_status) {
        sleep(2); // This sleep is to let registration finish
        app_instance = pfd->GetAppInstance(app_id);
    } else {
        app_instance = new ApplicationInstance(app_id, 0, 0, 0);
    }
    LOG1("got app instance");
    if (get_status) {
        sleep(4); // This sleep is to let registration finish
        usleep(GetRandomSleep()); // This sleep is to be random
    }
    LOG1("inducing failure");
    double strt = GetRealTime();
    InduceFailure(failure);
    double induced = GetRealTime();
    app_status status;
    if (get_status) {
        do {
            status = pfd->GetDownStatus(app_instance);
            //LOG1(status2str[status]);
        } while (status != DOWN_FROM_HANDLER &&
                 status != DOWN_FROM_VMM &&
                 status != DOWN_FROM_SWITCH);
        double detected = GetRealTime();
        std::printf("%d\t%f\t%f\n", status, induced - strt, detected - induced);
    } else {
        std::printf("%f\t%f\n", strt, induced);
    }
    return 0;
}
