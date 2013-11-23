#include "client_prot.h"
#include "client.h"

#include <arpa/inet.h>
#include <stdint.h>

#include "common.h"

extern void *StopThreadHelper(void *);

bool_t
client_null_1_svc(void* argp, void* result, struct svc_req *rqstp) {
    bool_t ret;
    FalconClient* cl = FalconClient::GetInstance();
    cl->Lock();
    if (cl->addr2layer_[svc_getcaller(rqstp->rq_xprt)->sin_addr.s_addr]) {
        ret = 1;
    } else {
        LOG("ignoring: %s", inet_ntoa(svc_getcaller(rqstp->rq_xprt)->sin_addr));
        ret = 0;
    }
    cl->Unlock();
    return ret;
}

bool_t
client_up_1_svc(client_up_arg* argp, void* result, struct svc_req *rqstp) {
    LOG("Got up %s", argp->handle);
    FalconClient* cl = FalconClient::GetInstance();
    cl->Lock();
    FalconLayer* layer = cl->addr2layer_[svc_getcaller(rqstp->rq_xprt)->sin_addr.s_addr];
    LOG("%s", inet_ntoa(svc_getcaller(rqstp->rq_xprt)->sin_addr));
    if (layer) {
        LOG("Doing up %s", argp->handle);
        layer->RegisterUp(argp->handle, argp->generation.generation_len,
                          argp->generation.generation_val);
    }
    cl->Unlock();
    return 1;
}

bool_t
client_down_1_svc(client_down_arg* argp, void* result, struct svc_req *rqstp) {
    LOG("Got down %s", argp->handle);
    FalconClient* cl = FalconClient::GetInstance();
    cl->Lock();
    FalconLayer* layer = cl->addr2layer_[svc_getcaller(rqstp->rq_xprt)->sin_addr.s_addr];
    if (layer) {
        LOG("Doing down %s", argp->handle);
                layer->RegisterDown(argp->handle, argp->generation.generation_len,
                                    argp->generation.generation_val, argp->layer_status,
                                    argp->killed, argp->would_kill);
        for (unsigned int i = 0; i < addrs->size(); i++) {
            cl->addr2layer_[(*addrs)[i]] = NULL;
        }
        delete addrs;
        LOG("at layer %s layer->GetNDependents() %d", layer->GetName().c_str(), layer->GetNDependents());
        if (layer->GetNDependents() == 0) {
            std::vector<std::string>* name = new std::vector<std::string>;
            do {
                name->push_back(layer->GetName());
                layer = layer->GetParent();
            } while (layer != cl->base_layer_);
            pthread_t gc;
            pthread_create(&gc, NULL, StopThreadHelper, name);
            pthread_detach(gc);
        }
        LOG1("Launched thread");
    }
    cl->Unlock();
    return 1;
}

int
client_prog_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result) {
    return 1;
}
