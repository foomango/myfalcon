#include "common/common.h"
#include "common/messages.h"
#include "common/config.h"
#include "pfd/pfd.h"
#include <unistd.h>

Application*
GetApplicationId(const char *filename) {
    std::string app_handle, host_name, vmm_name, switch_name;
    long handlerd_delay;
    long vmspy_delay;
    long switch_delay;
    long handlerd_spy_timeout;
    long handlerd_confirm_timeout;
    long vmspy_timeout;
    long switch_timeout;
    Config::LoadConfig(filename);
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

int
main (int argc, char **argv) {
    PFD *pfd = PFD::Instance();
    /*
    if (argc != 7) {
        printf("usage: %s app_handle vmname vmmname router_name qps time\n", argv[0]);
    } */
    if (argc != 2) {
        printf("usage: %s config_file\n", argv[0]);
    }
    Application *app_id = GetApplicationId(argv[1]);
    ApplicationInstance* app_instance = pfd->GetAppInstance(app_id);
    //uint32_t qps = atoi(argv[5]);
    uint32_t qps;
    double time;
    CHECK(Config::GetFromConfig("qps", &qps));
    CHECK(Config::GetFromConfig("qps", &time));
    Config::DeleteConfig();

    double spq = 1.0/qps;
    double uspq = spq * 1e6;
    double begin = GetRealTime();
    double stop = begin + time;
    uint32_t count = 0;
    if (qps == 0) {
        uspq = 1000 * 1000;
    }
    for (;;) {
        double b = GetRealTime();
        if (qps != 0) pfd->GetAppStatus(app_instance);
        double e = GetRealTime();
        int32_t sleep_time = ((int32_t)(uspq - ((e-b) * 1e6)));
        if (sleep_time < 0) {
            sleep_time = 0;
        }
        usleep(sleep_time);
        count++;
        if (e > stop) break;
    }
    if (qps == 0) {
        count = 0;
    }
    double end = GetRealTime();
    printf("did %f qps\n", count/(end - begin));
    printf("sent %lu bytes\n", count * sizeof(struct handlerd_query));
    printf("recv %lu bytes\n", count * sizeof(struct handlerd_query_response));
    return 0;
}
