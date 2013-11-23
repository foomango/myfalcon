#ifndef _NTFA_PFD_APPLICATION_INSTANCE_
#define _NTFA_PFD_APPLICATION_INSTANCE_

#include "common/common.h"
#include "pfd/Application.h"
#include <pthread.h>

class PFD;

// Application instance will also need to be refcounted by the PFD because some spy 
// threads may hold references after the calling process queries the instance. A
// SpyThreadInfo object or the PFD (at ReleaseApplicationInstance) will get rid of the 
// object.
class ApplicationInstance {
    public:
        ApplicationInstance(Application *app_id,
                            uint32_t application_generation,
                            uint32_t domain_generation,
                            uint32_t hypervisor_generation) { 
            application_id_ = app_id;
            application_generation_number_ = application_generation;
            domain_generation_number_ = domain_generation;
            hypervisor_generation_number_ = hypervisor_generation;
            ref_count_ = 1;
            pthread_mutex_init(&instance_lock_, NULL);
        }
        ~ApplicationInstance() {
            pthread_mutex_destroy(&instance_lock_);
            delete application_id_;
        }
        Application *GetApplicationIdentifier() {
            return application_id_;
        }
        uint32_t GetApplicationGenerationNumber() const {
            return application_generation_number_;
        }
        uint32_t GetDomainGenerationNumber() const {
            return domain_generation_number_;
        }
        uint32_t GetHypervisorGenerationNumber() const {
            return hypervisor_generation_number_;
        }
        int DecrementReferenceCount() {
            int ret;
            pthread_mutex_lock(&instance_lock_);
            ref_count_--;
            ret = ref_count_;
            pthread_mutex_unlock(&instance_lock_);
            return ret;
        }
        void IncrementReferenceCount() {
            pthread_mutex_lock(&instance_lock_);
            ref_count_++;
            pthread_mutex_unlock(&instance_lock_);
            return;
        }


    private:
        Application*    application_id_;
        uint32_t        application_generation_number_;
        uint32_t        domain_generation_number_;
        uint32_t        hypervisor_generation_number_;
        pthread_mutex_t instance_lock_;
        int             ref_count_;
};

#endif // _NTFA_PFD_APPLICATION_INSTANCE_
