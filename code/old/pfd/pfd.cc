#include "pfd/pfd.h"

#include <arpa/inet.h>
#include <inttypes.h>
#include <libvirt/libvirt.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <map>
#include <string>


#include "common/common.h"
#include "common/messages.h"
#include "pfd/Application.h"
#include "pfd/ApplicationInstance.h"
#include "pfd/SpyThreadInfo.h"

static const int32_t kJSONBufSize = 4096;
static const int32_t kBufferSize = 1024;
PFD*        PFD::PFDInstance_ = NULL; // singleton
bool        PFD::initialized_ = false;
std::map<std::string,int> application_handlerd_map_;
pthread_mutex_t handlerd_map_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t callback_map_lock_ = PTHREAD_MUTEX_INITIALIZER;
std::map<ApplicationInstance*,SpyThreadInfo*> callback_map_;


const char *status2str[NUM_STATUS] = {
    "confirmed dead from handler",
    "unknown at handler",
    "confirmed dead from vmspy",
    "unknown at vmspy",
    "confirmed dead from switch",
    "unknown at switch",
    "could not find handler",
    "could not find vmm",
    "could not find switch",
    "up"
};

PFD*
PFD::Instance(void) {
    if (!initialized_) {
        initialized_ = true;
    }
    return PFDInstance_;
}

// Ugh, unfortunately there is still a race condition:
// T1: Get handler sock fd = 5
// T2: Get handler sock fd = 5
// T1: Close handlerd sock
// T3: Get handler* sock fd = 5
// T2: Uh-oh, wrong handler!
// I don't know if this is worth fixing right now, but I'm leaving the
// note
void
PFD::CloseHandlerdSocket(const Application& app) {
    pthread_mutex_lock(&handlerd_map_lock_);
    const char* handlerd = app.GetAppString();
    int sock = application_handlerd_map_[handlerd];
    if (sock > 0) {
        close(sock);
    }
    application_handlerd_map_[handlerd] = -1;
    pthread_mutex_unlock(&handlerd_map_lock_);
    return;
}

int
PFD::GetHandlerdSocket(const Application& app) {
    pthread_mutex_lock(&handlerd_map_lock_);
    const char* handlerd = app.GetAppString();
    int sock = application_handlerd_map_[handlerd];
    if (sock <= 0) { 
        // Hmm... 0 might actually be valid...
        sock = ThreadsafeConnect(app.GetHostname(), kHandlerdPortNum);
    }
    application_handlerd_map_[handlerd] = sock;
    pthread_mutex_unlock(&handlerd_map_lock_);
    return sock;
}

int
PFD::ThreadsafeConnect(const char *hostname, const uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sock >= 0);
    struct addrinfo hints;
    struct addrinfo* ret;
    memset(&hints, '\0', sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(0 != getaddrinfo(hostname, NULL, &hints, &ret)) {
    	close(sock);
        return -1;
    }
    reinterpret_cast<struct sockaddr_in *>(ret->ai_addr)->sin_port = htons(port);
    CHECK(ret->ai_family == AF_INET);
    if (0 != connect(sock, ret->ai_addr, ret->ai_addrlen)) {
        close(sock);
        freeaddrinfo(ret);
        return -1;
    }
    freeaddrinfo(ret);
    int on = 1;
    CHECK(0 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)));
    return sock;
}

int
PFD::HandlerdConnect(const Application& app) {
    return GetHandlerdSocket(app);
}

enum spy_state_e
PFD::TryHandlerd(ApplicationInstance &app_instance, uint32_t *last_gen) {
    spy_state_e state = OTHER_ERROR;
    Application* app = app_instance.GetApplicationIdentifier();
    /* Talk with handlerd */
    int sock = HandlerdConnect(*app);
    if (sock < 0) return state;
    struct handlerd_query handler_query;
    CopyHandle(handler_query.dst_handle, app->GetHandle());
    handler_query.spy_timeout_ms = htonl(app->GetHandlerdSpyTimeout());
    handler_query.confirm_wait_ms = htonl(app->GetHandlerdConfirmTimeout());
    handler_query.app_generation = htonl(app_instance.GetApplicationGenerationNumber());
    handler_query.domain_generation = htonl(app_instance.GetDomainGenerationNumber());
    handler_query.hypervisor_generation = htonl(app_instance.GetHypervisorGenerationNumber());
    int ret_val = send(sock, &handler_query, sizeof(handler_query), 0);
    LOG("sent %d bytes to handlerd", ret_val);
    if (sizeof(handler_query) != ret_val) {
            LOG1("failed to query remote handlerd");
            CloseHandlerdSocket(*app);
            return state; // No idea what error this is
    }
    struct handlerd_query_response handler_reply;
    int recv_ret = recv(sock, &handler_reply, sizeof(handler_reply), 0);
    LOG("received %d bytes from handlerd", recv_ret);
    if (recv_ret == sizeof(handler_reply)) {
        char handler_state = handler_reply.state;
        if (last_gen != NULL) {
            *last_gen = ntohl(handler_reply.last_generation);
        }
        switch (handler_state) {
            case 'A':
                return RESPONDED_UP;
            case 'D':
                return RESPONDED_DOWN;
            case 'U':
                return RESPONDED_UNCONFIRMED;
            case 'I':
                return RESPONDED_UNKNOWN;
            default:
                LOG("default response %c in handler", handler_state);
                return OTHER_ERROR;
        }
    } else if (recv_ret == 0) {
        LOG1("failed to query remote handlerd");
    }
    CloseHandlerdSocket(*app);
    return OTHER_ERROR;
}

// This returns a pointer with allocated memory. Must free the memory (or at
// least keep a reference around)
virConnectPtr
PFD::VMMConnect(Application const &app) {
    virConnectPtr conn = NULL;
    char buf[kBufferSize];
    // TODO(leners): less magic?
    sprintf(buf, "qemu+tcp://%s:16509/system", app.GetVMMHostname());
    conn = virConnectOpen(buf);
    return conn;
}

enum spy_state_e
PFD::TryVMM(ApplicationInstance &app_instance, uint32_t *last_gen) {
    Application* app = app_instance.GetApplicationIdentifier();
    if (!app->HasVMM()) return NOCONNECTION;
    virConnectPtr vmm_conn = VMMConnect(*app);
    if (vmm_conn == NULL) return NOCONNECTION;
    virDomainPtr domain = virDomainLookupByName(vmm_conn, app->GetHostname());
    if (domain == NULL) {
        virConnectClose(vmm_conn);
        return OTHER_ERROR; // OTHER_ERROR
    }
    // If last_gen != NULL, this is a query for generation number
    if (last_gen != NULL) {
        LOG1("querying libvirtd for gen_no")
        uint32_t gen = virDomainGetNtfaGeneration(domain);
        virConnectClose(vmm_conn);
        virDomainFree(domain);
        *last_gen = gen;
        if (gen == 0) {
            LOG1("Error getting generation");
            return OTHER_ERROR;
        }
        return RESPONDED_DOWN;
    }
    LOG1("querying libvirtd")
    int ret_val = virDomainDoNtfaProbe(domain, app->GetVMSpyTimeout(),
                                       app_instance.GetDomainGenerationNumber(),
                                       app_instance.GetHypervisorGenerationNumber());
    virConnectClose(vmm_conn);
    virDomainFree(domain);

    LOG("got %d from libvirtd", ret_val)
    switch (ret_val) {
        case NTFA_ERROR:
            return OTHER_ERROR;
        case NTFA_NOCONNECTION:
            return NOCONNECTION;
        case NTFA_INACTIVE: // TODO(leners): Check this one
        case NTFA_UNKNOWN:
            return RESPONDED_UNKNOWN;
        case NTFA_DEAD:
        case NTFA_LONG_DEAD:
            return RESPONDED_DOWN;
        case NTFA_ALIVE:
            return RESPONDED_UP;
        case -1: // if the connection totally dies, this seems to happen
            return OTHER_ERROR;
        default:
            if (ret_val == -1) LOG1("ret_val is -1");
            CHECK(0);
    }
    return OTHER_ERROR;
}

int
PFD::SwitchConnect(Application* app) {
    int sock = ThreadsafeConnect(app->GetSwitchName(), kQueryPort); 
    return sock;
}

enum spy_state_e
PFD::TrySwitch(ApplicationInstance &app_instance, uint32_t *last_gen) {
    Application* app = app_instance.GetApplicationIdentifier();
    if (!app->HasSwitch()) {
        LOG1("No Switch!");
        return NOCONNECTION;
    }
    int switch_sock = SwitchConnect(app);
    if (switch_sock == -1) {
    	LOG1("Couldn't convert switch addr");
        return NOCONNECTION;
    }
    const char *hostname = app->GetVMMHostname();
    struct router_query query;
    memset(&query, 0, sizeof(query));
    strcpy(query.hostname, hostname);
    query.timeout_ms = htonl(app->GetRouterSpyTimeout());
    query.generation_num = htonl(app_instance.GetHypervisorGenerationNumber());
    int ret_val = send(switch_sock, &query, sizeof(query), 0);
    LOG("sent %d bytes to switch", ret_val);
    if (ret_val != sizeof(query)) {
    	LOG1("Error sending router query");
        close(switch_sock);
        return OTHER_ERROR;
    }
    struct router_response resp;
    ret_val = recv(switch_sock, &resp, sizeof(resp), 0);
    LOG("received %d bytes from switch", ret_val);
    close(switch_sock);
    if (ret_val < 0) {
        return OTHER_ERROR;
    }
    if (last_gen != NULL) {
        *last_gen = ntohl(resp.last_generation);
    }
    if (resp.state == 'D') {
        return RESPONDED_DOWN;
    } else if (resp.state == 'A') {
        return RESPONDED_UP;
    } else if (resp.state == 'I') {
        return RESPONDED_UNKNOWN;
    }
    return OTHER_ERROR;
}


static void*
PthreadSpyHandler(void* sti, spy_type st) {
    SpyThreadInfo* spy_info = (SpyThreadInfo*) sti;

    ApplicationInstance* app = spy_info->GetApp();
    Application* app_id = app->GetApplicationIdentifier();

    long delay_ms;
    switch(st) {
        case HANDLERD:
            delay_ms = app_id->GetHandlerdStartDelay();
            break;
        case VMM_SPY:
            delay_ms = app_id->GetVMSpyStartDelay();
            break;
        case SWITCH_SPY:
            delay_ms = app_id->GetRouterSpyStartDelay();
            break;
    }

    int rc = spy_info->WorkerWaitMilliseconds(delay_ms);
    if(rc == ETIMEDOUT) {   
        switch(st) {
            case HANDLERD:
                spy_info->GetHandlerdState();
                break;
            case VMM_SPY: 
                spy_info->GetVMMState();
                break;
            case SWITCH_SPY:
                spy_info->GetSwitchState();
                break;
        }
    }
    if(spy_info->DecrementReferenceCount() == 0) { 
        delete spy_info; 
    }
    return NULL;
}

static void*
PthreadHandlerdHandler(void* spy_info) {
    return PthreadSpyHandler(spy_info, HANDLERD);
}

static void*
PthreadVMMHandler(void* spy_info) {
    return PthreadSpyHandler(spy_info, VMM_SPY); 
}

static void*
PthreadRouterHandler(void* spy_info) {
    return PthreadSpyHandler(spy_info, SWITCH_SPY);
}

void
PFD::RegisterCallbacks(ApplicationInstance *app) {
    pthread_mutex_lock(&callback_map_lock_);
    if (callback_map_[app] == NULL) {
        pthread_t handlerd_pthread, vmm_pthread, router_pthread;
        SpyThreadInfo* spy_info = new SpyThreadInfo(app, this, STATUS_QUERY);
        callback_map_[app] = spy_info;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&handlerd_pthread, &attr,
                       PthreadHandlerdHandler, (void*)spy_info);
        pthread_create(&vmm_pthread, &attr,
                       PthreadVMMHandler, (void*)spy_info);
        pthread_create(&router_pthread, &attr,
                       PthreadRouterHandler, (void*)spy_info);
    }
    pthread_mutex_unlock(&callback_map_lock_);
    return;
}

app_status
PFD::GetDownStatus(ApplicationInstance *app) {
    pthread_mutex_lock(&callback_map_lock_);
    SpyThreadInfo *info = callback_map_[app];
    pthread_mutex_unlock(&callback_map_lock_);
    return info->GetFinalAppStatus();
}


app_status
PFD::GetAppStatus(ApplicationInstance *app) {
    app_status ret;
    pthread_mutex_lock(&callback_map_lock_);
    ret = callback_map_[app]->GetCurrAppStatus();
    pthread_mutex_unlock(&callback_map_lock_);
    return ret;
}

void
PFD::ReleaseAppInstance(ApplicationInstance* app) {
    if (app->DecrementReferenceCount() == 0) {
        delete app;
    }
    return;
}

ApplicationInstance*
PFD::GetAppInstance(Application* app_id) {
    return GetAppInstance(app_id, false);
}

ApplicationInstance*
PFD::GetAppInstance(Application* app_id, bool beakless) {
    // A fake AppInstance for fetching generation numbers
    Application* fake_app_id = new Application(app_id->GetHandle(),
                                               app_id->GetHostname(),
                                               app_id->GetVMMHostname(),
                                               app_id->GetSwitchName());
    ApplicationInstance* fake_app = new ApplicationInstance(fake_app_id, 0, 0, 0);
    SpyThreadInfo* spy_info = new SpyThreadInfo(fake_app, this, GENERATION_QUERY);
    pthread_t app_thread, domain_thread, hypervisor_thread;
    fake_app_id->SetHandlerdStartDelay(0);
    fake_app_id->SetVMSpyStartDelay(0);
    fake_app_id->SetRouterSpyStartDelay(0);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&app_thread, &attr, PthreadHandlerdHandler, (void*)spy_info);
    pthread_create(&domain_thread, &attr, PthreadVMMHandler, (void*)spy_info);
    pthread_create(&hypervisor_thread, &attr, PthreadRouterHandler, (void*)spy_info);
    gen_numbers gen_set = spy_info->GetGenerationNumbers();
    if (beakless) {
        uint32_t beakmask = (1 << (sizeof(uint32_t) * 8 - 1));
        gen_set.hypervisor_gen |= beakmask;
    }
    ApplicationInstance* app = new ApplicationInstance(app_id,
        gen_set.app_gen, gen_set.domain_gen, gen_set.hypervisor_gen);
    if (spy_info->DecrementReferenceCount() == 0) {
        delete spy_info;
    }
    RegisterCallbacks(app);
    ReleaseAppInstance(fake_app);
    return app;
}

