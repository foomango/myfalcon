#ifndef _NTFA_PFD_PFD_
#define _NTFA_PFD_PFD_

#include <libvirt/libvirt.h>

#include "common/common.h"
#include "pfd/Application.h"
#include "pfd/ApplicationInstance.h"

enum spy_state_e {
    RESPONDED_UP,
    RESPONDED_DOWN,
    RESPONDED_UNCONFIRMED,
    RESPONDED_UNKNOWN,
    TIMEDOUT,
    NOCONNECTION,
    OTHER_ERROR,
    INIT_STATE  
};

typedef enum spy_type_e {
    HANDLERD,
    VMM_SPY,
    SWITCH_SPY
} spy_type;

typedef struct gen_numbers_e {
    uint32_t app_gen;
    uint32_t domain_gen;
    uint32_t hypervisor_gen;
} gen_numbers;

// This shows the intention when querying a spy.
// Currently we have two types: status_query and generation_number_query.
typedef enum query_type_e {
    STATUS_QUERY,
    GENERATION_QUERY
} query_type;

extern const char *status2str[NUM_STATUS];

class PFD {
    public:
        // Singleton
        static PFD*             Instance();
        app_status              GetAppStatus(ApplicationInstance* app); 
        app_status              GetDownStatus(ApplicationInstance* app); 
        enum spy_state_e        TryHandlerd(ApplicationInstance& app,
                                            uint32_t* last_gen = NULL);
        enum spy_state_e        TryVMM(ApplicationInstance& app,
                                       uint32_t* last_gen = NULL);
        enum spy_state_e        TrySwitch(ApplicationInstance& app,
                                          uint32_t* last_gen = NULL);
        ApplicationInstance*    GetAppInstance(Application* app_id);
        ApplicationInstance*    GetAppInstance(Application* app_id, bool beakless);
        void                    ReleaseAppInstance(ApplicationInstance* app);

    private:
        int                     ThreadsafeConnect(const char *hostname,
                                                  const uint16_t port);
        int                     HandlerdConnect(const Application &app);
        virConnectPtr           VMMConnect(const Application &app);
        enum spy_state_e        DoVMMHandshake(int sock);
        enum spy_state_e        SendVMMProbe(int sock, uint32_t delay_ms);
        enum spy_state_e        WaitForVMMResponse(int sock);
        int                     SwitchConnect(Application* app);
        int                     GetHandlerdSocket(const Application& app);
        void                    CloseHandlerdSocket(const Application& app);
        void                    RegisterCallbacks(ApplicationInstance* app);
        static bool initialized_;
        static PFD* PFDInstance_;
};
#endif  // _NTFA_PFD_PFD_
