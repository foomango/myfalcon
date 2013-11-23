#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <pthread.h>

/* dbug */
#include <arpa/inet.h>
#include <netinet/in.h>
/* end dbug */

#include <algorithm>
#include <boost/shared_ptr.hpp>

#include "common.h"
#include "client.h"
#include "spy_prot.h"

struct RegistrationInfo {
    RegistrationInfo(FalconClient* c, std::vector<std::string> h, bool l, falcon_callback fcb, int32_t e2eto) :
        cl(c), handle(h), lethal(l), cb(fcb), e2etimeout(e2eto) {};
    FalconClient*               cl;
    std::vector<std::string>    handle;
    bool                        lethal;
    falcon_callback_fn          cb;
    int32_t                     e2etimeout;
};


void client_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);
// Singleton state
bool FalconClient::initialized_ = false;
FalconClient* FalconClient::client_ = NULL;
bool FalconClient::done_init_ = false;
uint32_t FalconClient::starting_ = 0;
bool FalconClient::stopping_ = false;

pthread_mutex_t FalconClient::client_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t FalconClient::client_cond_ = PTHREAD_COND_INITIALIZER;
pthread_cond_t FalconClient::init_cond_ = PTHREAD_COND_INITIALIZER;
pthread_cond_t FalconClient::timer_cond_ = PTHREAD_COND_INITIALIZER;

FalconClient*
FalconClient::GetInstance () {
    if (initialized_) {
        return client_;
    }
    initialized_ = true;
    client_ = new FalconClient();
    client_->Init();
    return client_;
}

const uint32_t kHostnameSize = 128;
void
FalconClient::SvcThread() {
    Lock();
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    char namebuf[kHostnameSize];
    CHECK(0 == gethostname(namebuf, kHostnameSize));
    CHECK(HostLookup(std::string(namebuf), &addr_));
    socklen_t len = sizeof(addr_);
    CHECK(0 == bind(fd, (sockaddr *) &addr_, len));
    srv_trans_ = svcudp_create(fd);
    CHECK(srv_trans_);
    CHECK(1 == svc_register(srv_trans_, CLIENT_PROG, CLIENT_V1, client_prog_1, 0));
    CHECK(0 == getsockname(fd, (sockaddr *)&addr_, &len));
    done_init_ = true;
    LOG("listening on %u", ntohs(addr_.sin_port));
    pthread_cond_signal(&init_cond_);
    Unlock();
    svc_run(); // Never returns
}

void*
SvcThreadHelper(void *) {
    FalconClient* cl = FalconClient::GetInstance();
    cl->SvcThread();
    return NULL;
}

void*
TimerThreadHelper(void *) {
    FalconClient* cl = FalconClient::GetInstance();
    cl->TimerThread();
    return NULL;
}

void*
RegistrationThreadHelper(void *arg) {
    pthread_detach(pthread_self());
    RegistrationInfo* i = static_cast<RegistrationInfo*>(arg);
    i->cl->StartRegister();
    i->cl->DoRegistration(i);
    i->cl->StopRegister();
    delete i;
    return NULL;
}

void
FalconClient::StartRegister() {
    Lock();
    while (stopping_) {
        CHECK(0 == pthread_cond_wait(&client_cond_, &client_lock_));
    }
    starting_++;
    Unlock();
}

void
FalconClient::StopRegister() {
    Lock();
    CHECK(starting_ > 0);
    starting_--;
    if (starting_ == 0) pthread_cond_broadcast(&client_cond_);
    Unlock();
}

void
FalconClient::BeginStop() {
    Lock();
    LOG("%d %s", starting_, stopping_ ? "true" : "false");
    while (starting_ > 0 || stopping_) {
        pthread_cond_wait(&client_cond_, &client_lock_);
    }
    stopping_ = true;
    Unlock();
}

void
FalconClient::EndStop() {
    Lock();
    stopping_ = false;
    pthread_cond_broadcast(&client_cond_);
    Unlock();
}

void
FalconClient::CallDown(const std::vector<uint32_t>& addrs,
                       const std::vector<uint32_t>& gen,
                       const std::vector<std::string>& names) {
    LOG("%lu %lu %lu", addrs.size(), gen.size(), names.size());
    uint32_t *gen_vec = new uint32_t[gen.size()];
    std::copy(gen.begin(), gen.end(), gen_vec);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kFalconPort);
    for (uint32_t i = 0; i < addrs.size(); i++) {
        if (addrs[i] == 0) break;
        timeval wait;
        memset(&wait, 0, sizeof(timeval));
        wait.tv_sec = kFalconRetry;
        addr.sin_addr.s_addr = addrs[i];
        // Is this the right idea?
        uint32_t index = names.size() - addrs.size() + i;
        LOG("would try to kill %s at %s", names[names.size() - addrs.size() + i].c_str(), inet_ntoa(addr.sin_addr));
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        CHECK(fd >= 0);
        CLIENT* clnt = clntudp_create(&addr, SPY_PROG, SPY_V1, wait, &fd);
        CHECK(clnt >= 0);
        spy_cancel_arg arg;
        spy_res res;
        memset(&arg, 0, sizeof(arg));
        memset(&res, 0, sizeof(res));
        arg.target.handle = new char[names[index].size() + 1];
        memcpy(arg.target.handle, names[index].c_str(),
                names[index].size() + 1);
        arg.target.generation.generation_val = gen_vec;
        arg.target.generation.generation_len = gen.size() - index;
        arg.client.ipaddr = addr_.sin_addr.s_addr;
        arg.client.port = addr_.sin_port;
        wait.tv_sec *= 5;
        enum clnt_stat st =
            clnt_call(clnt, SPY_CANCEL, (xdrproc_t) xdr_spy_cancel_arg, 
                      (caddr_t) &arg, (xdrproc_t) xdr_spy_res, (caddr_t) &res,
                      wait);
        LOG("CANCEL res: %d %d", st, res.status);
        delete [] arg.target.handle;
        xdr_free((xdrproc_t) xdr_spy_res, (char *)&res);
        clnt_destroy(clnt);
        close(fd);
    }
    delete [] gen_vec;
    return;
}

void
FalconClient::StopThread(std::vector<std::string>* vec) {
    BeginStop();
    Lock();
    EndStop();
    return;
}

void *
StopThreadHelper(void *arg) {
    FalconClient* cl = FalconClient::GetInstance();
    cl->StopThread(static_cast<std::vector<std::string>*>(arg));
    return NULL;
}

FalconLayerPtr
FalconClient::BuildLayer(FalconLayer* parent,
                              const std::string& handle,
                              std::vector<uint32_t>* gen_vec,
                              bool lethal,
                              FalconCallbackPtr cb,
                              bool last_layer = false) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    FalconLayerPtr c();
    if (parent == base_layer_) {
        if (HostLookup(handle, &addr)) {
            c = FalconLayerPtr(new FalconLayer(handle, std::vector<uint32_t>(),
                                               addr_, base_layer_, lethal,
                                               timeout, &addr, cb));
        }
    } else {
        c = parent->AddTarget(handle, gen_vec, lethal, cb, last_layer);
    }
    return c;
}

bool
FalconClient::RegisterLayer(FalconLayerPtr parent, FalconLayerPtr target, bool lethal, uint32_t to) {
    Lock();
    parent->Register(target->handle, lethal, to);
    Unlock();
    // Do call
    wait.tv_sec *= kFalconRetryLim;
    enum clnt_stat st =
        clnt_call(clnt, SPY_REGISTER, (xdrproc_t) xdr_spy_register_arg,
                  (caddr_t) &arg, (xdrproc_t) xdr_spy_res, (caddr_t) &res,
                  wait);
    // Cleanup
    status_t fst = res.status;
    delete [] arg.target.handle;
    delete [] arg.target.generation.generation_val;
    xdr_free((xdrproc_t) xdr_spy_res, (char *)&res);
    clnt_destroy(clnt);
    close(fd);

    return (st == RPC_SUCCESS && fst == FALCON_REGISTER_ACK);
}

void
FalconClient::DoRegistration(RegistrationInfo *info) {
    FalconLayer* curr_layer = base_layer_;
    std::vector<uint32_t> gen;
    // Get generation and build tree
    for (int i = info->handle.size() - 1; i >= 0; i--) {
        // 1. Check whether this layer exists
        FalconLayer* next_layer = curr_layer->GetDependent(info->handle[i]);
        if (next_layer != NULL) {
            if (i == 0) info->cb(info->handle, 15, 15);
            curr_layer = next_layer;
            LOG("%s", next_layer->GetName().c_str());
            gen = next_layer->GetGen();
            Unlock();
            return;
        }
        // Unlock because this call will block.
        next_layer = BuildLayer(curr_layer, info->handle[i], &gen, info->lethal, i == 0, info->cb);
        if (next_layer == NULL) {
            // Do the callback
            LOG("couldn't build layer %s %d", info->handle[i].c_str(), i);
            CHECK(0);
            info->cb(info->handle, 12, 13);
            return;
        }
        if (next_layer->GetAddr() != 0) {
            addr2layer_[next_layer->GetAddr()] = next_layer;
        }
        // This will delete the potentially duplicated layer
        curr_layer->SetDependent(info->handle[i], next_layer);
        curr_layer = curr_layer->GetDependent(info->handle[i]);
    } 
    // Register callbacks
    curr_layer = base_layer_;
    for (int i = info ->handle.size() - 2; i >= 0; i--) {
        curr_layer = curr_layer->GetDependent(info->handle[i+1]);
        FalconLayer* next_layer = curr_layer->GetDependent(info->handle[i]);
        CHECK(next_layer != NULL);
        // TODO(leners): fix magic!
        if (!RegisterLayer(curr_layer, next_layer, info->lethal, (i == 0) ? info->e2etimeout/5 : -1)) {
            info->cb(info->handle, 14, 15);
        }
    }
    ClientEvent* ev = new ClientEvent(info->handle, gen, WAIT_UP);
    timespec tmp;
    clock_gettime(CLOCK_REALTIME, &tmp);
    tmp.tv_sec += info->e2etimeout;
    ev->SetTime(&tmp);
    pq_.push(ev);
    pthread_cond_signal(&timer_cond_);
    Unlock();
    LOG("Done with registration");
}

void
FalconClient::Unlock() {
    pthread_mutex_unlock(&client_lock_);
}

void
FalconClient::Lock() {
    pthread_mutex_lock(&client_lock_);
}

void
FalconClient::GetNextEventTime(timespec* t) {
    pq_.top()->GetTimeSpec(t);
}

void
FalconClient::StopMonitoring(const std::vector<std::string>& handle) {
    std::vector<std::string>* name = new std::vector<std::string>(handle);
    pthread_t tmp;
    pthread_create(&tmp, NULL, StopThreadHelper, name);
    pthread_detach(tmp);
    return;
}

void
FalconClient::StartMonitoring(const std::vector<std::string>& handle,
                              bool lethal, falcon_callback_fn cb,
                              int32_t e2etimeout) {
    pthread_t tmp;
    FalconCallbackPtr fcb = FalconCallbackPtr(new FalconCallback(handle, cb));
    RegistrationInfo* info = new RegistrationInfo(this, handle, lethal, fcb,
                                                  e2etimeout);
    pthread_create(&tmp, NULL, RegistrationThreadHelper, info);
    return;
}

ClientEvent*
FalconClient::GetNextEvent() {
    Lock();
    ClientEvent* ret = pq_.top();
    pq_.pop();
    Unlock();
    return ret;
}

void
FalconClient::InsertEvent(ClientEvent* ev) {
    pq_.push(ev);
    return;
}

void
FalconClient::TimerThread() {
    for (;;) {
        while (pq_.empty()) {
            pthread_cond_wait(&timer_cond_, &client_lock_);
        }
        int res = 0;
        while (GetRealTime() < pq_.top()->GetTime()) {
            timespec next_time;
            pq_.top()->GetTimeSpec(&next_time);
            res = pthread_cond_timedwait(&timer_cond_, &client_lock_, &next_time);
        }
        ClientEvent *ev = pq_.top();
        pq_.pop();
        FalconLayer* l = GetLayer(ev->GetHandle(), ev->GetGen());
        if (l == NULL) continue;
        if (ev->GetType() == WAIT_UP) {
            timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            if (l->CheckTime(&now)) {
                LOG("Timed out! %s", l->GetName().c_str());
                std::vector<uint32_t>* ret = new std::vector<uint32_t>;
                FalconLayer* p = l->GetParent();
                if (!l->Lethal()) {
                    l->DoCallback(0, 0, ret);
                    for (unsigned int i = 0; i < ret->size(); i++) {
                        addr2layer_[(*ret)[i]] = NULL;
                    }
                } else {
                    // create a thread that sends a kill @ parent and waits
                    ev->SetType(WAIT_DOWN);
                    pq_.push(ev);
                    continue;
                }
                p->RemoveDependent(l->GetName());
                delete ret;
                delete l;
                delete ev;
            } else {
                timespec to;
                l->GetTimeout(&to);
                ev->SetTime(&to);
                pq_.push(ev);
            }
        } else if (ev->GetType() == WAIT_DOWN) {
            LOG("Gonna try and kill %s @ %s", l->GetName().c_str(), l->GetParent()->GetName().c_str());
            CHECK(0);
            // 1. Create CLNT*
            // 2. Create kill_arg
            // 3. Fork killer thread (remember we don't care about its result 
            // for now) if it's not an ACK, we probably need to do something.
            // 4. Add new WAIT_DOWN event for the next layer down. 
        }
    }
    return;
}

FalconLayer*
FalconClient::GetLayer(const std::vector<std::string>& name, const std::vector<uint32_t>& gen) {
    FalconLayer* curr_layer = base_layer_;
    for (int i = static_cast<int>(name.size()) - 1; curr_layer != NULL && i >= 0; i--) {
        curr_layer = curr_layer->GetDependent(name[i]);
    }
    if (!curr_layer || !curr_layer->CompareGen(gen)) {
        return FalconLayerPtr();
    }
    return curr_layer;
}

void
FalconClient::Init () {
    pthread_cond_init(&timer_cond_, NULL);

    base_layer_ = FalconLayerPtr(new FalconLayer());

    // Create timer thread
    pthread_create(&timer_, NULL, TimerThreadHelper, NULL);
    pthread_detach(timer_);

    // Start RPC server
    pthread_create(&srv_, NULL, SvcThreadHelper, NULL);
    pthread_detach(srv_);
    
    Lock();
    while (!done_init_) {
        pthread_cond_wait(&init_cond_, &client_lock_);
    }
    Unlock();
    return;
}
