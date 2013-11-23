#ifndef _NTFA_CLIENT_CLIENT_H_
#define _NTFA_CLIENT_CLIENT_H_

#include "client_prot.h"
#include "layer.h"

#include <map>
#include <queue>
#include <string>
#include <vector>

const int32_t kDefaultFalconTimeout = 10 * 5; // Five minutes

struct RegistrationInfo; 

typedef enum ce_type {
    WAIT_UP,
    WAIT_DOWN
} ev_type;

class ClientEvent {
    public:
        ClientEvent() {};
        ClientEvent(const std::vector<std::string>& handle,
                    const std::vector<uint32_t>& gen,
                    ev_type type) : handle_(handle), gen_(gen), type_(type) {};
        
        bool operator() (const ClientEvent* l, const ClientEvent* r) const {
            return l->GetTime() > r->GetTime();
        }

        double GetTime() const {
            return t_.tv_sec + (1e-9) * t_.tv_nsec;
        }
        
        void SetTime(timespec *t) {
            memcpy(&t_, t, sizeof(timespec));
            return;
        }

        void GetTimeSpec(timespec* t) const {
            memcpy(t, &t_, sizeof(timespec));
            return;
        }

        const std::vector<std::string>& GetHandle() const {
            return handle_;
        }

        const std::vector<uint32_t>& GetGen() const {
            return gen_;
        }

        ev_type GetType() const {
            return type_;
        }

        void SetType(ev_type type) {
            type_ = type;
            return;
        }

    private:
        std::vector<std::string>  handle_;
        std::vector<uint32_t>     gen_;
        ev_type                         type_;
        timespec                        t_;
};

class FalconClient {
    public:
        static FalconClient* GetInstance();

        ~FalconClient() {
            initialized_ = false;
        };
        // Start monitoring 
        void StartMonitoring(const std::vector<std::string>& handle,
                             bool lethal,
                             falcon_callback cb,
                             int32_t e2etimeout = kDefaultFalconTimeout);
        // Stop Monitoring
        void StopMonitoring(const std::vector<std::string>& handle);

        friend bool_t client_up_1_svc(client_up_arg*, void*, struct svc_req*);
        friend bool_t client_down_1_svc(client_down_arg*, void*, struct svc_req*);
        friend bool_t client_null_1_svc(void*, void*, struct svc_req*);
        friend void* TimerThreadHelper(void *);
        friend void* SvcThreadHelper(void *);
        friend void* RegistrationThreadHelper(void *);
        friend void* StopThreadHelper(void *);
        friend int  main();
    
    protected:
        // This is the layer corresponding to the PFD.
        FalconLayerPtr                      base_layer_;
        std::map<uint32_t,FalconLayer*>     addr2layer_;
        void                                GetNextEventTime(timespec* t);
        ClientEvent*                        GetNextEvent();
        void                                Lock();
        void                                Unlock();
        void                                TimerThread();
        void                                SvcThread();
        void                                StartRegister();
        void                                StopRegister();
        void                                StopThread(std::vector<std::string>*);
        void                                BeginStop();
        void                                EndStop();
    
    private:
        FalconClient() {};
        void Init();
        void InsertEvent(ClientEvent *ev);
        FalconLayerPtr GetLayer(const std::vector<std::string>&, const std::vector<uint32_t>&);
        FalconLayerPtr BuildLayer(FalconLayer*, const std::string&, std::vector<uint32_t>*, bool, bool, falcon_callback);
        void DoRegistration(RegistrationInfo *);
        bool RegisterLayer(FalconLayer *, FalconLayer *, bool, uint32_t);
        void CallDown(const std::vector<uint32_t>& addrs,const std::vector<uint32_t>& gen, const std::vector<std::string>& names);

        // Singleton stuff
        static bool                     initialized_;
        static FalconClient*            client_;
        static pthread_cond_t           init_cond_;
        static bool                     done_init_;

        // Asynchronous callback stuff
        pthread_t                           srv_; 
        SVCXPRT*                            srv_trans_;
        static pthread_mutex_t              client_lock_;
        sockaddr_in                         addr_;

        // Start/Stop (only one op can be in progress at a time)
        static pthread_cond_t                      client_cond_;
        static uint32_t                            starting_;
        static bool                                stopping_;

        // Timeout thread stuff
        pthread_t                           timer_;
        static pthread_cond_t                      timer_cond_;
        // Doesn't need to be static:
        std::priority_queue<ClientEvent*, std::vector<ClientEvent*>, ClientEvent> pq_;

};
#endif // _NTFA_CLIENT_CLIENT_H_
