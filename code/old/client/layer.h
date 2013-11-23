#ifndef _NTFA_CLIENT_LAYER_H_
#define _NTFA_CLIENT_LAYER_H_

#include <rpc/rpc.h>
#include <stdint.h>
#include <time.h>

#include <boost/smart_ptr.hpp>

#include <map>
#include <string>
#include <vector>

typedef void(*falcon_callback_fn)(const std::vector<std::string>&, uint32_t, uint32_t);

class FalconCallback {
    public:
        FalconCallback(falcon_callback_fn f, const std::vector<std::string>& h) {
            f_ = f;
            h_ = h;
        }
        void operator() (uint32_t falcon_status, uint32_t remote_status) {
            f_(h_, falcon_status, remote_status);
        }
    private:
        falcon_callback_fn f_;
        std::vector<std::string> h_;
};

class FalconLayer;
typedef boost::shared_ptr<FalconCallback> FalconCallbackPtr;
typedef boost::shared_ptr<FalconLayer> FalconLayerPtr;
typedef boost::weak_ptr<FalconLayer> FalconParentPtr;

namespace {
// Settings for the falcon calls. Retry after 1 second, repeate at most 5 times.
const timespec kFalconRetry = {1, 0};
const timespec kFalconRetryLim = {5, 0};
}

class FalconLayer {
    public:
        FalconLayer() :
            lethal_(false), has_clnt_(false) {};
        
        FalconLayer(const std::string& name, 
                    const std::vector<uint32_t>& gen_vec, sockaddr_in id_addr,
                    FalconLayerPtr parent, bool lethal, uint32_t timeout,
                    sockaddr_in* addr, FalconCallbackPtr cb) :
                    name_(name), gen_no_(gen_vec), addr_(id_addr), 
                    parent_(parent), lethal_(lethal), timeout_(timeout),
                    callback_(cb), has_clnt_(false) {
            if (addr) {
                timeval wait = {kFalconRetry, 0};
                has_clnt_ = true;
                s_ = socket(AF_INET, SOCK_DGRAM, 0);
                CHECK(s_ >= 0);
                clnt_ = clntudp_create(addr, SPY_PROG, SPY_V1, wait, &s);
                CHECK(clnt_);
            }
        }

        ~FalconLayer() {
            if (has_clnt_) {
                clnt_destroy(clnt_);
                close(s_);
            }
        }

        void RegisterUp(const char*, uint32_t, const uint32_t*);
        void RegisterDown(const char*, uint32_t, const uint32_t*,
                                            uint32_t, bool, bool);

        bool CheckTime(const timespec *);
        void GetTimeout(timespec *);
        void SetLastUpTime();
        void DoCallback(uint32_t, uint32_t, std::vector<uint32_t>*);

        clnt_stat Cancel(const char* handle) {
            spy_cancel_arg arg;
            spy_res res;
            memset(&arg, 0, sizeof(arg));
            memset(&res, 0, sizeof(res));
            arg.target.handle = handle;
            arg.target.generation.generation_val = gen_no_;
            arg.target.generation.generation_len = gen_no_.size();
            arg.client.ipaddr = addr_.sin_addr.s_addr;
            arg.client.port = addr_.sin_port;
            enum clnt_stat st =
                clnt_call(clnt, SPY_CANCEL, (xdrproc_t) xdr_spy_cancel_arg, 
                          (caddr_t) &arg, (xdrproc_t) xdr_spy_res, (caddr_t) &res,
                          wait);
        
        }

        FalconLayerPtr
        AddTarget(const std::string& handle, bool lethal,
                  FalconCallbackPtr cb, bool last_layer) {
            spy_get_gen_arg arg;
            spy_res res;
            memset(&arg, 0, sizeof(arg));
            memset(&res, 0, sizeof(res));
            arg.target.handle = new char[handle.size() + 1];
            memcpy(arg.target.handle, handle.c_str(),
                   handle.size() + 1);
            enum clnt_stat st =
                clnt_call(clnt_, SPY_GET_GEN, (xdrproc_t) xdr_spy_get_gen_arg,
                          (caddr_t) &arg, (xdrproc_t) xdr_spy_res, (caddr_t) &res,
                          kFalconRetryLim);
            if (st != RPC_SUCCESS) {
                LOG("%s:%d\n", handle.c_str(), st);
                CHECK(0);
            } 
            if (res.status != FALCON_GEN_RESP) {
                LOG("Bad resp: %s:%d\n", handle.c_str(), res.status);
                CHECK(0);
            }
            delete [] arg.target.handle;
            xdr_free((xdrproc_t) xdr_spy_res, (char *)&res);
            std::vector<uint32_t> gen(res.target.generation.generation_val, 
                                      res.target.generation.generation_val +
                                      res.target.generation.generation_len);            
            
            // Construct the child. Getting our own FalconLayerPtr is a bit
            // hoopty
            FalconLayerPtr child = FalconLayerPtr(new FalconLayer(handle, gen,
                                                  addr_, 
                                                  parent_->GetDependent(name_),
                                                  lethal, timeout, addr, cb));
            dependents_[handle] = child;

            // Check generation consistency. TODO(leners) something more graceful.
            CHECK(CheckGen(handle.c_str(), 
                           res.target.generation.generation_len,
                           res.target.generation.generation_val));
            return child;
        }

        const std::vector<uint32_t>& GetGen() const {
            return gen_no_;
        }

        size_t       GetNDependents() const { return dependents_.size();}
        FalconLayerPtr GetDependent(const std::string&);
        bool         SetDependent(const std::string&, FalconLayer*);
        void         RemoveDependent(const std::string&);

        bool CompareGen(const std::vector<uint32_t>&) const;

        FalconLayerPtr GetParent() const { return parent_.lock(); }
        const std::string& GetName() const { return name_;}
        bool Lethal() const {return lethal_;}


    private:
        // Check a dependent's generation
        bool CheckGen(const char*, uint32_t, const uint32_t*);

        // client identifying address for RPC calls
        sockaddr_in addr_;

        // Data for all layer types
        const std::string                       name_;
        const std::vector <uint32_t>            gen_no_;
        FalconParentPtr                            parent_;
        std::list<FalconCallbackPtr> callbacks_;
        std::map<std::string, FalconLayerPtr>     dependents_;
        bool                                    lethal_;
        uint32_t                                timeout_;
        timespec                                last_up_time_;
};


#endif // _NTFA_CLIENT_LAYER_H_
