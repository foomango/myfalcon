    void ping_cb(viewid_t vid, ref<ping_res> resp, clnt_stat stat) {
	if (view_.vid != vid)
	    return;

	if (stat) {
	    ping_stop();
	    view_is_down();
	    return;
	}

	if (!resp->ok) {
	    /*
	     * This is in case we're fetching logs from the new primary,
	     * and we're stepping through a number of views quickly.
	     * Timeout will reset us only if we fail to reach the final
	     * view within periodic_primary_ping.
	     */
	    ping_stop();
	    ping_timeout_cb_ =
		delaycb(periodic_primary_ping,
			wrap(mkref(this), &cohort::view_is_down));
	}
    }

    void ping_check_incoming() {
	if (ping_incoming_.size() != view_.backups.size() + 1)
	    view_is_down();
	else
	    ping_start();
    }

    void ping_stop() {
	if (ping_timeout_cb_) {
	    timecb_remove(ping_timeout_cb_);
	    ping_timeout_cb_ = 0;
	}
    }

    void ping_start() {
	if (self_.mid == view_.primary.mid) {
	    ping_incoming_.clear();
	    ping_timeout_cb_ =
		delaycb(periodic_primary_ping + timeout_primary_ping,
			wrap(mkref(this), &cohort::ping_check_incoming));
	} else {
	    ping_timeout_cb_ =
		delaycb(periodic_primary_ping,
			wrap(mkref(this), &cohort::ping_start));
	}

	ping_arg arg;
	arg.myid = self_.mid;
	arg.vid = view_.vid;
	sockaddr_in sin = netaddr2sockaddr(view_.primary.addr);
	ref<ping_res> resp = New refcounted<ping_res>();
	c_->timedcall(timeout_primary_ping, REP_PING, &arg, resp,
		      wrap(mkref(this), &cohort::ping_cb, view_.vid, resp),
		      0, 0, 0, 0, 0, (sockaddr *) &sin);
    }

    void ping_restart() {
	ping_stop();
	ping_start();
    }

		ping_restart();
case REP_PING: {
	    ping_arg *argp = sbp->Xtmpl getarg<ping_arg> ();
	    ping_res res;
	    res.ok = valid_view_ && argp->vid == view_.vid;
	    if (res.ok)
		ping_incoming_.insert(argp->myid);
	    sbp->reply(&res);
	    break;
	}

    set<mid_t> ping_incoming_;
    timecb_t *ping_timeout_cb_;
