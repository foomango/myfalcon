#ifndef _NTFA_PFD_APPLICATION_
#define _NTFA_PFD_APPLICATION_
#include "common/common.h"
#include <string>

const int32_t kMaxHostnameLength = 32;

class Application {
    public:
        Application(const char* handle, const char* hostname) {
            CopyHandle(handle_, handle);
            CopyHandle(hostname_, hostname);
            app_string_ = std::string(handle_);
            app_string_.append(":");
            app_string_.append(hostname_);
            has_vmspy_ = false;
            has_switch_ = false;

            handlerd_id_ = hostname_;
            vmm_id_ = "";
            switch_id_ = "";
        
            handlerd_start_delay_ms_ = kDefaultHandlerdStartDelay;
            vmspy_start_delay_ms_ = kDefaultVMSpyStartDelay;
            router_spy_start_delay_ms_ = kDefaultRouterSpyStartDelay;

            handlerd_spy_timeout_ms_ = kDefaultHandlerdSpyTimeout;
            handlerd_confirm_timeout_ms_ = kDefaultHandlerdConfirmTimeout;
            vmspy_timeout_ms_ = kDefaultVMSpyTimeout;
            router_spy_timeout_ms_ = kDefaultRouterSpyTimeout;
        }
        Application(const char*handle, const char* hostname,
                    const char *vmm_hostname) {
            CopyHandle(handle_, handle);
            CopyHandle(hostname_, hostname);
            CopyHandle(vmm_hostname_, vmm_hostname);
            app_string_ = std::string(handle_);
            app_string_.append(":");
            app_string_.append(hostname_);
            app_string_.append(":");
            app_string_.append(vmm_hostname_);
            has_vmspy_ = true;
            has_switch_ = false;
            
            handlerd_id_ = hostname_;
            handlerd_id_.append(":");
            handlerd_id_.append(vmm_hostname_);
            vmm_id_ = vmm_hostname_;
            switch_id_ = "";
            
            handlerd_start_delay_ms_ = kDefaultHandlerdStartDelay;
            vmspy_start_delay_ms_ = kDefaultVMSpyStartDelay;
            router_spy_start_delay_ms_ = kDefaultRouterSpyStartDelay;

            handlerd_spy_timeout_ms_ = kDefaultHandlerdSpyTimeout;
            handlerd_confirm_timeout_ms_ = kDefaultHandlerdConfirmTimeout;
            vmspy_timeout_ms_ = kDefaultVMSpyTimeout;
            router_spy_timeout_ms_ = kDefaultRouterSpyTimeout;
        }
        Application(const char* handle, const char* hostname,
                    const char* vmm_hostname, const char* switch_name) {
            CopyHandle(handle_, handle);
            CopyHandle(hostname_, hostname);
            CopyHandle(vmm_hostname_, vmm_hostname);
            CopyHandle(switch_hostname_, switch_name);
            app_string_ = std::string(handle_);
            app_string_.append(":");
            app_string_.append(hostname_);
            app_string_.append(":");
            app_string_.append(vmm_hostname_);
            app_string_.append(":");
            app_string_.append(switch_hostname_);
            has_vmspy_ = true;
            has_switch_ = true;

            handlerd_id_ = hostname_;
            handlerd_id_.append(":");
            handlerd_id_.append(vmm_hostname_);
            handlerd_id_.append(":");
            handlerd_id_.append(switch_hostname_);
            vmm_id_ = vmm_hostname_;
            vmm_id_.append(":");
            vmm_id_.append(switch_hostname_);
            switch_id_ = switch_hostname_;

            handlerd_start_delay_ms_ = kDefaultHandlerdStartDelay;
            vmspy_start_delay_ms_ = kDefaultVMSpyStartDelay;
            router_spy_start_delay_ms_ = kDefaultRouterSpyStartDelay;

            handlerd_spy_timeout_ms_ = kDefaultHandlerdSpyTimeout;
            handlerd_confirm_timeout_ms_ = kDefaultHandlerdConfirmTimeout;
            vmspy_timeout_ms_ = kDefaultVMSpyTimeout;
            router_spy_timeout_ms_ = kDefaultRouterSpyTimeout;
        }
        const char *GetAppString(void) const {
            return app_string_.c_str();
        }
        const char *GetHandlerdId(void) const {
            return handlerd_id_.c_str();
        }
        const char *GetVMMId(void) const {
            return vmm_id_.c_str();
        }
        const char *GetSwitchId(void) const {
            return switch_id_.c_str();
        }
        const char *GetHostname(void) const {
            return hostname_;
        }
        const char *GetHandle(void) const {
            return handle_; // Since handle_t is an array
        }
        const char *GetVMMHostname(void) const {
            return vmm_hostname_;
        }
        bool HasVMM(void) const {
            return has_vmspy_;
        }
        const char *GetSwitchName(void) const {
            return switch_hostname_;
        }
        bool HasSwitch(void) const {
            return has_switch_;
        }
        void SetHandlerdStartDelay(long delay_ms) {
            handlerd_start_delay_ms_ = delay_ms;
        }
        void SetVMSpyStartDelay(long delay_ms) {
            vmspy_start_delay_ms_ = delay_ms;
        }
        void SetRouterSpyStartDelay(long delay_ms) {
            router_spy_start_delay_ms_ = delay_ms;
        }
        long GetHandlerdStartDelay() const {
            return handlerd_start_delay_ms_;
        }
        long GetVMSpyStartDelay() const {
            return vmspy_start_delay_ms_;
        }
        long GetRouterSpyStartDelay() const {
            return router_spy_start_delay_ms_;
        }
        void SetHandlerdSpyTimeout(long delay_ms) {
            handlerd_spy_timeout_ms_ = delay_ms;
        }
        void SetHandlerdConfirmTimeout(long delay_ms) {
            handlerd_confirm_timeout_ms_ = delay_ms;
        }
        void SetVMSpyTimeout(long delay_ms) {
            vmspy_timeout_ms_ = delay_ms;
        }
        void SetRouterSpyTimeout(long delay_ms) {
            router_spy_timeout_ms_ = delay_ms;
        }
        long GetHandlerdSpyTimeout() const {
            return handlerd_spy_timeout_ms_;
        }
        long GetHandlerdConfirmTimeout() const {
            return handlerd_confirm_timeout_ms_;
        }
        long GetVMSpyTimeout() const {
            return vmspy_timeout_ms_;
        }
        long GetRouterSpyTimeout() const {
            return router_spy_timeout_ms_;
        }
    private:
        handle_t    handle_;
        char        hostname_[kMaxHostnameLength];
        char        vmm_hostname_[kMaxHostnameLength];
        bool        has_vmspy_;
        char        switch_hostname_[kMaxHostnameLength];
        bool        has_switch_;
        std::string app_string_;
        std::string handlerd_id_;
        std::string vmm_id_;
        std::string switch_id_;
        
        long handlerd_start_delay_ms_;
        long vmspy_start_delay_ms_;
        long router_spy_start_delay_ms_;

        long handlerd_spy_timeout_ms_;
        long handlerd_confirm_timeout_ms_;
        long vmspy_timeout_ms_;
        long router_spy_timeout_ms_;
};
#endif  // _NTFA_PFD_APPLICATION_
