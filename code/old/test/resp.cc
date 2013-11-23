#include "common/common.h"
#include "common/config.h"
#include "pfd/pfd.h"
#include <unistd.h>
#include <stdio.h>

double *array = NULL;
const int chunkSize = 4096;
const int doublesPerChunk = 4096/sizeof(double);
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
    if (argc != 2) {
        fprintf(stderr, "usage: %s config_file", argv[0]);
    }
    //Application* app_id = new Application(argv[1], argv[2], argv[3], argv[4]);
    Application* app_id = GetApplicationId(argv[1]);
    ApplicationInstance* app_instance = pfd->GetAppInstance(app_id);
    //double duration = atof(argv[5]);
    double duration;
    CHECK(Config::GetFromConfig("time", &duration));
    Config::DeleteConfig();

    // For now exclude spies lower than handlerd
    int i = 0;
    // Establish connection
    app_status start_status = pfd->GetAppStatus(app_instance);
    CHECK(start_status == UP || start_status == DOWN_FROM_HANDLER || start_status == UNKNOWN_FROM_HANDLER);
    double strt = GetRealTime();
    for (;; ++i) {
        if ( ((i) % doublesPerChunk) == 0) {
            void *new_array = realloc(array, (i / doublesPerChunk + 1) * chunkSize);
            if (new_array == NULL) {
                LOG1("out of memory");
                break;
            } else {
                array = (double *) new_array;
            }
        }
        double before = GetRealTime();
        app_status status = pfd->GetAppStatus(app_instance);
        if (start_status != status) {
            LOG("handlerd status changed %d\n", status);
            break;
        }
        double after = GetRealTime();
        array[i] = after - before;
        if (after - strt > duration) break;
    }
    int j;
    for (j = 0; j < i; j++) {
        // Print to stdout, script will redirect.
        printf("%f\n", array[j]);
    }
    return 0;
}
