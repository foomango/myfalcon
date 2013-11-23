/* 
 * Copyright (c) 2011 Joshua B. Leners (University of Texas at Austin).
 * All rights reserved.
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of Texas at Austin. The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 */
#include "enforcer.h"

#include <sfslite/async.h>

#include "common.h"

class DummyEnforcer : public virtual Enforcer {
  public:
    virtual ~DummyEnforcer() {}

    virtual void Init() {
        monitored_.insert("alive");
        monitored_.insert("dead");
        monitored_.insert("stuck");
        monitored_.insert("pb1");
        logfile_name_ = "/tmp/dummy.log";
        return;
    }

    virtual void StartMonitoring(const ref<const str> handle) {
        LOG("START MONITORING %s", handle->cstr());
        timecb_t* cb = delaycb(0, 100 * 1000 * 1000, wrap(mkref(this),
                               &DummyEnforcer::MonitorAction, handle));
        monitored_cb_[*handle] = cb;
        return;
    }

    virtual void StopMonitoring(const ref<const str> handle) {
        LOG("STOP MONITORING %s", handle->cstr());
        timecb_t* cb = monitored_cb_[*handle];
        if (cb == NULL) {
            return;
        }
        monitored_cb_[*handle] = NULL;
        timecb_remove(cb);
        return;
    }

    virtual bool InvalidTarget(const ref<const str> handle) {
        return (monitored_.count(*handle) == 0);
    }

    virtual void Kill(const ref<const str> handle) {
        CHECK(1 == monitored_.count(*handle));
        LOG("Got kill for %s", handle->cstr());
        ObserveDown(handle, 0, true, true);
        return;
    }

    virtual void UpdateGenerations(const ref<const str> handle) {
        return;
    }

    void MonitorAction(const ref<const str> handle) {
        timecb_t* cb = monitored_cb_[*handle];
        if (cb == NULL) {
            return;
        }
        monitored_cb_[*handle] = NULL;
        if (handle->cmp("dead") == 0) {
            bool killed = Killable(handle);
            ObserveDown(handle, 0, killed, true);
            return;
        } else if (handle->cmp("stuck") == 0) {
            return;
        }
        ObserveUp(handle);
        cb = delaycb(0, 100 * 1000 * 1000, wrap(mkref(this),
                     &DummyEnforcer::MonitorAction, handle));
        monitored_cb_[*handle] = cb;
        return;
    }
  private:
    std::map<str, timecb_t*>    monitored_cb_;
    std::set<str>               monitored_;
};

int
main() {
    async_init();
    ref<DummyEnforcer> e = New refcounted<DummyEnforcer>;
    e->Init();
    e->Run();
    return EXIT_FAILURE;
}
