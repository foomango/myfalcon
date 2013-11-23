#ifndef _NTFA_PFD_SPY_THREAD_INFO_
#define _NTFA_PFD_SPY_THREAD_INFO_

#include "common/common.h"
#include "pfd/pfd.h"

const uint32_t kRetrySleepUS = 5000; // to keep the pfd from hurting itself

class SpyThreadInfo {
    public:
        SpyThreadInfo(ApplicationInstance* app, PFD* pfd, query_type type) {
            app->IncrementReferenceCount();
            app_ = app;
            pfd_ = pfd;
            query_type_ = type;
            handlerd_state_ = INIT_STATE;
            vmm_state_ = INIT_STATE;
            switch_state_ = INIT_STATE;
            ref_count_ = kNumLayers + 1;

            pthread_mutex_init(&info_lock_, NULL);
            pthread_cond_init(&main_cond_, NULL);
            pthread_cond_init(&worker_cond_, NULL);
        }
        
        ~SpyThreadInfo() {
            pthread_mutex_destroy(&info_lock_);
            pthread_cond_destroy(&main_cond_);
            pthread_cond_destroy(&worker_cond_);
            if (app_->DecrementReferenceCount() == 0) {
                delete app_;
            }
        }
        
        int DecrementReferenceCount() {
            int ret = -1;
            pthread_mutex_lock(&info_lock_);
            ref_count_--;
            ret = ref_count_;
            pthread_mutex_unlock(&info_lock_);
            return ret;
        }
       
        // jbl: Should we rate limit retry in the following three functions?
        // Also, there has to be a better way to do this!
        spy_state_e GetHandlerdState() {
            uint32_t gen = 0;
            spy_state_e tmp_state = pfd_->TryHandlerd(*app_, &gen);
            pthread_mutex_lock(&info_lock_);  
            handlerd_state_ = tmp_state;
            generation_.app_gen = gen;
            bool ready_to_return = ReadyToReturn() ||
                                   (query_type_ == GENERATION_QUERY &&
                                    tmp_state == RESPONDED_DOWN);
            pthread_cond_signal(&main_cond_);
            pthread_mutex_unlock(&info_lock_);
            // Keep trying until we are ready to return
            if (ready_to_return) {
                return handlerd_state_;
            } else {
                usleep(kRetrySleepUS);
                return GetHandlerdState();
            }
        }
        
        spy_state_e GetVMMState() {
            uint32_t gen = 0;
            uint32_t* gen_p = (query_type_ == GENERATION_QUERY) ? &gen : NULL;
            spy_state_e tmp_state = pfd_->TryVMM(*app_, gen_p);
            pthread_mutex_lock(&info_lock_);
            vmm_state_ = tmp_state;
            generation_.domain_gen = gen;
            bool ready_to_return = ReadyToReturn() ||
                                   (query_type_ == GENERATION_QUERY &&
                                    tmp_state == RESPONDED_DOWN);
            pthread_cond_signal(&main_cond_);
            pthread_mutex_unlock(&info_lock_);
            if (ready_to_return) {
                return vmm_state_;
            } else {
                usleep(kRetrySleepUS);
                return GetVMMState();
            }
        }
        
        spy_state_e GetSwitchState() {
            uint32_t gen = 0;
            spy_state_e tmp_state = pfd_->TrySwitch(*app_, &gen);
            pthread_mutex_lock(&info_lock_);
            generation_.hypervisor_gen = gen;
            switch_state_ = tmp_state;
            pthread_cond_signal(&main_cond_);
            pthread_mutex_unlock(&info_lock_);
            return switch_state_;
        }
        
        ApplicationInstance* GetApp() {
            return app_;
        }

	app_status GetFinalAppStatus() {
	    app_status ret;
	    pthread_mutex_lock(&info_lock_);
	    while (!ReadyToReturn()) {
	    	pthread_cond_wait(&main_cond_, &info_lock_);
	    }
	    ret = GetAppStatus();
	    pthread_mutex_unlock(&info_lock_);
	    return ret;
	}
        
        app_status GetCurrAppStatus() {
            app_status ret;
            pthread_mutex_lock(&info_lock_);
            if (ReadyToReturn()) {
                ret = GetAppStatus();
            } else {
                ret = UP;
            }
            pthread_mutex_unlock(&info_lock_);
            return ret;
        }

        gen_numbers GetGenerationNumbers() {
            pthread_mutex_lock(&info_lock_);
            int rc = 0;
            while (rc == 0 && !ReadyToReturn()) {
                rc = pthread_cond_wait(&main_cond_, &info_lock_);
            }
            CHECK(ReadyToReturn());
            gen_numbers ret = generation_;
            pthread_cond_broadcast(&worker_cond_);
            pthread_mutex_unlock(&info_lock_);
            return ret;
        }


        int WorkerWaitMilliseconds(long delay_ms) {
            timespec tp;
            clock_gettime(CLOCK_REALTIME, &tp);
            IncrementTimespecMilliseconds(&tp, delay_ms);
            pthread_mutex_lock(&info_lock_);
            int rc = 0;
            // pthread_cond_timedwait may return spuriously, need to check that
            // we are returning either because it is cleanup time or because we
            // timed out
            while (rc == 0 && !ReadyToReturn()) {
                rc = pthread_cond_timedwait(&worker_cond_, &info_lock_, &tp);
            }
            // If we hold the lock, unlock it.
            if (rc == 0 || rc == ETIMEDOUT) {
                pthread_mutex_unlock(&info_lock_);
            }
            return rc;
        }

    private:
        ApplicationInstance* app_;
        PFD* pfd_;
        spy_state_e handlerd_state_;
        spy_state_e vmm_state_;
        spy_state_e switch_state_;
        pthread_mutex_t info_lock_;
        pthread_cond_t main_cond_;
        pthread_cond_t worker_cond_;
        gen_numbers generation_;
        int ref_count_;
        query_type query_type_;

        bool ReadyToReturn() {
            // the return value will depend on the query type.
            switch (query_type_) {
                case STATUS_QUERY:
                    return StatusReady();
                case GENERATION_QUERY:
                    return GenerationNumberReady();
            }
            return true;
        }

        bool StatusReady() {
            // application status is ready when
            // 1. handlerd returns a valid state
            // 2. vmm or switch return down
            // 3. all of 3 spies have returned
            return (handlerd_state_ != INIT_STATE &&
                    handlerd_state_ != OTHER_ERROR &&
                    handlerd_state_ != NOCONNECTION &&
                    handlerd_state_ != TIMEDOUT) ||
                   (vmm_state_ == RESPONDED_DOWN) ||
                   (switch_state_ == RESPONDED_DOWN);
        }

        bool GenerationNumberReady() {
            // generation number is ready when
            // 1. all of 3 spies have returned
            return (handlerd_state_ == RESPONDED_DOWN &&
                    vmm_state_ == RESPONDED_DOWN &&
                    switch_state_ == RESPONDED_DOWN);
        }

        app_status GetAppStatus() {
            if (handlerd_state_ == RESPONDED_UP) {
                return UP;
            } else if (handlerd_state_ == RESPONDED_DOWN) {
                return DOWN_FROM_HANDLER;
            } else if (handlerd_state_ == RESPONDED_UNKNOWN ||
                       handlerd_state_ == RESPONDED_UNCONFIRMED) {
                return UNKNOWN_FROM_HANDLER;
            }
            if (vmm_state_ == RESPONDED_UP) {
                CHECK(0);
                return NO_HANDLER; // shouldn't happen for now.
            } else if (vmm_state_ == RESPONDED_DOWN) {
                return DOWN_FROM_VMM;
            } else if (vmm_state_ == RESPONDED_UNKNOWN) {
                return UNKNOWN_FROM_VMM;
            }
            if (switch_state_ == RESPONDED_DOWN) {
                return DOWN_FROM_SWITCH;
            } else if (switch_state_ == RESPONDED_UP || switch_state_ == RESPONDED_UNKNOWN) {
                return UNKNOWN_FROM_SWITCH;
            }
            return NO_SWITCH;
        }
};

#endif  // _NTFA_PFD_SPY_THREAD_INFO_
