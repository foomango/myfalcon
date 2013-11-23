#include "client/layer.h"

#include "common.h"

void
FalconLayer::RegisterUp(const char* handle, uint32_t gen_len,
                        const uint32_t* gen_vec) {
    if (!CheckGen(handle, gen_len, gen_vec)) {
        LOG1("Bad generation in up response");
        return;
    }
    // This is guaranteed to be non-null by CheckGen
    FalconLayerPtr target = dependents_[handle];
    target->SetLastUpTime();
    return;
}

void
FalconLayer::SetLastUpTime() {
    LOG("setting %s", name_.c_str());
    clock_gettime(CLOCK_REALTIME, &last_up_time_);
    LOG("last uptime is: %f", last_up_time_.tv_sec + 1e-9 * last_up_time_.tv_nsec);
    return;
}

FalconLayerPtr
FalconLayer::GetDependent(const std::string& handle) {
    return dependents_[handle];
}

bool
FalconLayer::SetDependent(const std::string& handle, FalconLayerPtr l) {
    if (dependents_[handle]) {
        return false;
    } else {
        dependents_[handle] = l;
    }
    return true;
}

void
FalconLayer::RemoveDependent(const std::string& handle) {
    dependents_.erase(handle);
    return;
}


void
FalconLayer::GetTimeout(timespec* t) {
    memcpy(t, &last_up_time_, sizeof(timespec));
    t->tv_sec += timeout_;
    return;
}


void
FalconLayer::RegisterDown(const char* handle, uint32_t gen_len,
                          const uint32_t* gen_vec, uint32_t status,
                          bool killed, bool would_kill) {
    if (!CheckGen(handle, gen_len, gen_vec)) {
        LOG("Bad generation in down response %d %lu", gen_len, gen_no_.size());
        return;
    }
    // This is guaranteed to be non-null by CheckGen
    FalconLayerPtr target = dependents_[handle];
    // TODO(leners) construct falcon status
    target->DoCallback(status, 0, ret);
    dependents_.erase(handle);
    return;
}

bool
FalconLayer::CheckTime(const timespec *t) {
    double diff_s = (t->tv_sec - last_up_time_.tv_sec) +
                    1e-9 * (t->tv_nsec - last_up_time_.tv_nsec);
    LOG("%s: %f %d", name_.c_str(), diff_s, timeout_);
    return diff_s > ((double) timeout_);
}

void
FalconLayer::DoCallback(uint32_t layer_status, uint32_t falcon_status,
                        std::vector<uint32_t>* addresses) {
    std::list<FalconCallbackPtr>::iterator it;
    for (it = callbacks_.begin(); it != callbacks.end(); it++) {
        (**it)(layer_status, falcon_status);
    }
    return;
}

// Compare generations of two layers
bool
FalconLayer::CompareGen(const std::vector<uint32_t>& v1) const {
    // Sanity check 2: The vectors are the same size.
    if (v1.size() != gen_no_.size()) return false;

    for (unsigned int i = 0; i < gen_no_.size(); i++) {
        if (v1[i] != gen_no_[i]) return false;
    }
    return true;
}

// Check the validity of a target
bool
FalconLayer::CheckGen(const char *handle, uint32_t gen_len,
                      const uint32_t* gen_vec) {

    // Sanity check 1: We are responsible for the target
    FalconLayer* target = dependents_[handle];
    if (!target) return false;
    
    LOG("target name: %s", target->GetName().c_str());
    // Get the vectors to compare
    std::vector<uint32_t> their_vec(gen_vec, gen_vec + gen_len);
    return target->CompareGen(their_vec);
}
