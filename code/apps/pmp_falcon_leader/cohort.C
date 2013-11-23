/* 
 * Copyright (C) 2011 David Mazieres (dm@uun.org)
 *               2011 Joshua B. Leners (leners@cs.utexas.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

/*
 * Implement one cohort in a replication group.
 */

#include <arpc.h>
#include <parseopt.h>
#include <itree.h>
#include <crypt.h>	/* just for str2wstr */
#include "rep.h"
#include "util.h"
#include "execbackend.h"
#include "bcast.h"
#include "exitcb.h"
#include "execute.h"

#include <list>
#include <map>

#include "messages.h"
#include "libfail/saboteur.h"
#include "process_enforcer/process_observer.h"
#include <pthread.h>

static void
ignore_bool (bool) { }
static void
ignore_int (int) { }

uint32_t
GetTimeMs() {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec * 1000 + t.tv_nsec/1000000;
}

callback<void, bool>::ref cbb_null (gwrap (ignore_bool));
enum {
    timeout_join_view = 15,	/* before resending REP_LETMEIN */
    timeout_vc_prepare = 3,	/* wait for VIEW_CHANGE replies */
    timeout_view_change = 15,	/* retry a view change attempt */

};

// Falcon state
std::map<mid_t,int> mid2fdfd_;
uint32_t the_count_ = 0;

template <class T>
struct ackset {
    set<T> received;
    set<T> expected;
};

std::list<mid_t> active_servers_;

enum request_state { RQ_LOGGED, RQ_COMMITTED, RQ_EXECUTING, RQ_EXECUTED };

struct request_log_entry {
    itree_entry<request_log_entry> link;
    ptr<ackset<mid_t> > ack;
    svccb *sbp;		/* client request, used when executed */
    enum request_state state;
    viewstamp_t vs;
    execute_arg arg;

    request_log_entry() : state(RQ_LOGGED) {}
};

struct reply_cache_entry {
    itree_entry<reply_cache_entry> link;
    mid_t client;
    rid_t rid;
    request_log_entry *rle;	/* if not committed/executed yet */
    str reply;
};

struct viewstamp_jump_entry {
    itree_entry<viewstamp_jump_entry> link;
    viewstamp_t prev;	/* last VS in an old view */
    viewstamp_t next;	/* first VS in a new view, should be ts==0 */
};

class cohort : public virtual refcount {
 public:
    cohort(uint16_t group_port, execute_t cb, sockaddr_in join_addr, const char* name, const char* hypervisor) : exec_(cb) {
	self_.mid = unique_process_id();
	memset(&proposed_vid_, 0, sizeof(proposed_vid_));
	ping_timeout_cb_ = 0;

	/*
	 * Allocate client FDs for both unicast and broadcast requests.
	 */
	int cfd = inetsocket(SOCK_DGRAM, 0, 0);
	assert(cfd >= 0);
	make_async(cfd);
	close_on_exec(cfd);
	c_ = aclnt::alloc(axprt_dgram::alloc(cfd), rep_prog_1, 0,
			  callbase_alloc<rpccb_unreliable_sockaddrin>);

	int bfd = bcast_info_t::bind_bcast_sock();
	assert(bfd >= 0);
	make_async(bfd);
	close_on_exec(bfd);
	bc_ = aclnt::alloc(axprt_dgram::alloc(bfd), rep_bcast_prog_1, 0,
			   callbase_alloc<rpccb_bcast>);

	/*
	 * Safety check: if we're going to form an entirely new group
	 * from a synthetic view (below), then broadcast a request on
	 * our group port to make sure someone else isn't using this
	 * port already.
	 */
	if (join_addr.sin_port == 0) {
	    sockaddr_in sin;
	    sin.sin_family = AF_INET;
	    sin.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	    sin.sin_port = htons(group_port);

	    bc_->call(REP_BCAST_NULL, 0, 0,
		      wrap(mkref(this), &cohort::warn_group_inuse, group_port),
		      0, 0, 0, 0, 0, (sockaddr *) &sin);
	}

	/*
	 * Allocate server FDs (unicast and broadcast) and start
	 * listening for requests on them.
	 */
	int fd = inetsocket(SOCK_DGRAM, 0, myipaddr());
	assert(fd >= 0);
	make_async(fd);
	close_on_exec(fd);

	ucast_srv_ = asrv::alloc(axprt_dgram::alloc(fd), rep_prog_1);
	ucast_srv_->setcb(wrap(mkref(this), &cohort::dispatch));

	int group_fd = bcast_info_t::bind_bcast_sock(group_port, true);
	assert(group_fd >= 0);
	make_async(group_fd);
	close_on_exec(group_fd);

	bcast_srv_ = asrv::alloc(axprt_dgram::alloc(group_fd), rep_bcast_prog_1);
	bcast_srv_->setcb(wrap(mkref(this), &cohort::dispatch_bcast));

	/*
	 * Figure out our own address
	 */
	sockaddr_in sin;
	socklen_t len = sizeof(sin);
	getsockname(fd, (sockaddr *) &sin, &len);

	self_.addr.ipaddr = ntohl(sin.sin_addr.s_addr);
	self_.addr.port = ntohs(sin.sin_port);
	assert(self_.addr.ipaddr);
	self_.name = rpc_str<RPC_INFINITY>(name);
	self_.hypervisor = rpc_str<RPC_INFINITY>(hypervisor);

	in_addr a;
	a.s_addr = htonl(self_.addr.ipaddr);
	warn << GetTimeMs() << ":"<< "Cohort running on " << inet_ntoa(a) << ":" << self_.addr.port << "\n";

	/*
	 * If we're starting this replication group,
	 * construct an initial synthetic view.
	 */
	if (join_addr.sin_port == 0) {
	    view_.vid.counter = 1;
	    view_.vid.manager = self_.mid;
	    view_.primary = self_;
	    view_.backups.setsize(0);

	    latest_.vid = view_.vid;
	    latest_.ts = 0;
	    committed_ = latest_;
	    executed_ = latest_;
	    valid_view_ = true;

	    view_form_t f;
	    memset(&f.prev, 0, sizeof(f.prev));	/* null prev is head of log */
	    f.view = view_;

	    request_log_entry *rle = new request_log_entry();
	    rle->ack = 0;
	    rle->sbp = 0;
	    rle->vs = latest_;

	    str fs = xdr2str(f);
	    rle->arg.client = 0;
	    rle->arg.rid = 0;
	    rle->arg.vid.counter = 0;
	    rle->arg.vid.manager = 0;
	    rle->arg.request.setsize(fs.len());
	    memcpy(rle->arg.request.base(), fs.cstr(), fs.len());

	    vs_jumps_update(rle);
	    log_.insert(rle);
	    mode_ = VC_ACTIVE;

            active_servers_.push_back(self_.mid);

	    warn << GetTimeMs() << ":"<< "Formed a new synthetic view.\n";
	} else {
	    latest_.vid.counter = 0;
	    latest_.vid.manager = 0;
	    latest_.ts = 0;
	    committed_ = latest_;
	    executed_ = latest_;

	    valid_view_ = false;
	    mode_ = VC_BOOTSTRAP;
	    join_view(join_addr);
	}
    }

 private:
    void warn_group_inuse(uint16_t port, clnt_stat stat) {
	if (stat == RPC_TIMEDOUT)
	    return;

	warn << GetTimeMs() << ":"<< "\n"
	     << "****************\n"
	     << "*** WARNING! ***\n"
	     << "****************\n"
	     << "\n"
	     << "This cohort formed a new group using port " << port << ",\n"
	     << "but there is already another process responding to broadcast\n"
	     << "requests on the same port.  Perhaps you forgot to tell this\n"
	     << "cohort to join that group; otherwise you may want to choose\n"
	     << "a different port number to avoid conflicts.\n"
	     << "\n";
    }

    bool is_view_member(mid_t mid) {
	return valid_view_ && view2mids(view_)->contains(mid);
    }

    void join_view(sockaddr_in join_addr) {
	if (is_view_member(self_.mid))
	    return;

	if (valid_view_)
	    join_addr = netaddr2sockaddr(view_.primary.addr);

	warn << GetTimeMs() << ":"<< "Trying to join view...\n";
	delaycb(timeout_join_view, wrap(mkref(this), &cohort::join_view, join_addr));
	if (join_addr.sin_addr.s_addr != htonl(INADDR_BROADCAST))
	    c_->call(REP_LETMEIN, &self_, 0, aclnt_cb_null,
		     0, 0, 0, 0, 0, (sockaddr *) &join_addr);
	else
	    bc_->call(REP_BCAST_LETMEIN, &self_, 0, aclnt_cb_null,
		      0, 0, 0, 0, 0, (sockaddr *) &join_addr);
    }

    void form_view(view_form_t f) {
	assert(f.view.primary.mid == self_.mid);

	latest_.vid = f.view.vid;
	latest_.ts = 0;
	committed_ = latest_;

	str s = xdr2str(f);
	replicate_arg ra;
	ra.vs = latest_;
	ra.arg.client = 0;
	ra.arg.rid = 0;
	ra.arg.vid.counter = 0;
	ra.arg.vid.manager = 0;
	ra.arg.request.setsize(s.len());
	memcpy(ra.arg.request.base(), s.cstr(), s.len());
	ra.committed = committed_;

	request_log_entry *rle = New request_log_entry();
	rle->sbp = 0;
	rle->ack = 0;
	rle->vs = latest_;
	rle->arg = ra.arg;
	vs_jumps_update(rle);
	log_.insert(rle);
	advance_committed();

	for (uint32_t i = 0; i < f.view.backups.size(); i++) {
	    cohort_t b = f.view.backups[i];
	    ref<replicate_res> resp = New refcounted<replicate_res>();
	    sockaddr_in sin = netaddr2sockaddr(b.addr);
	    c_->call(REP_REPLICATE, &ra, resp,
		     wrap(mkref(this), &cohort::replicate_cb, b.mid, resp),
		     0, 0, 0, 0, 0, (sockaddr *) &sin);
	}
    }

    struct view_change_attempt {
	/* cs244b: start cut for lab */
	set<mid_t> prepare_accepted;
	set<mid_t> prepare_done;
	set<mid_t> nv_accepted;
	set<mid_t> nv_done;
	vec<ref<view_change_res> > vcaccepts;

	ptr<view_t> newview;	/* as received from other nodes */
	viewid_t merged_vid;

	new_view_arg nvarg;
	/* cs244b: end cut for lab */
	/* Insert here any state that your view change manager should keep. */

	viewid_t proposed_vid;
	set<cohort_t> underlings;

	bool aborted;
    };

    /* cs244b: start cut for lab */
    void view_change_check_newview(ptr<view_change_attempt> attempt) {
	if (attempt->aborted)
	    return;

	if (!attempt->nv_accepted.contains_majority_of(*view2mids(attempt->nvarg.view)) ||
	    !attempt->nv_accepted.contains_majority_of(*view2mids(view_)))
	{
	    /* Everyone has either replied or timed out..  No dice. */
	    if (attempt->nv_done.size() == attempt->underlings.size())
		attempt->aborted = true;
	    return;
	}

	/*
	 * We've got a majority of both old and new views accepting us.
	 * Time to form a new view.
	 */
	view_form_t f;
	f.prev = attempt->nvarg.latest;
	f.view = attempt->nvarg.view;

	/* Could be sending a loopback RPC to ourselves, but that's fine */
	sockaddr_in sin = netaddr2sockaddr(attempt->nvarg.view.primary.addr);
	c_->call(REP_INIT_VIEW, &f, 0, aclnt_cb_null,
		 0, 0, 0, 0, 0, (sockaddr *) &sin);
    }

    void view_change_newview_cb(ptr<view_change_attempt> attempt, cohort_t c,
				ref<new_view_res> resp, clnt_stat stat) {
	attempt->nv_done.insert(c.mid);
	if (!stat && resp->accepted)
	    attempt->nv_accepted.insert(c.mid);
	view_change_check_newview(attempt);
    }

    void view_change_choose_view(ptr<view_change_attempt> attempt) {
	if (attempt->newview) {
	    ptr<set<mid_t> > newview_mids = view2mids(*attempt->newview);

	    attempt->nvarg.view.vid = attempt->proposed_vid;
	    attempt->nvarg.view.primary = attempt->newview->primary;
	    attempt->nvarg.view.backups.setsize(attempt->newview->backups.size());
	    for (uint32_t i = 0; i < attempt->newview->backups.size(); i++)
		attempt->nvarg.view.backups[i] = attempt->newview->backups[i];

	    /*
	     * If we haven't heard from attempt->newview->primary,
	     * swap its primary position with the backup having
	     * the highest latest field in its VIEW_CHANGE reply.
	     */
	    viewstamp_t max_latest;
	    cohort_t max_latest_c;
	    memset(&max_latest, 0, sizeof(max_latest));

	    for (uint32_t i = 0; i < attempt->vcaccepts.size(); i++) {
		ref<view_change_res> r = attempt->vcaccepts[i];
		if (!newview_mids->contains(r->accept->myid.mid))
		    continue;
		if (max_latest < r->accept->latest) {
		    assert(max_latest_c.mid != attempt->newview->primary.mid);
		    max_latest = r->accept->latest;
		    max_latest_c = r->accept->myid;
		}
	    }

	    attempt->nvarg.latest = max_latest;
	    if (max_latest_c.mid != attempt->newview->primary.mid) {
		for (uint32_t i = 0; i < attempt->nvarg.view.backups.size(); i++)
		    if (attempt->nvarg.view.backups[i].mid == max_latest_c.mid)
			attempt->nvarg.view.backups[i] = attempt->nvarg.view.primary;
		attempt->nvarg.view.primary = max_latest_c;
	    }

	    return;
	}

	viewstamp_t max_latest;
	cohort_t max_latest_c;

	memset(&max_latest, 0, sizeof(max_latest));

	for (uint32_t i = 0; i < attempt->vcaccepts.size(); i++) {
	    ref<view_change_res> r = attempt->vcaccepts[i];
	    if (max_latest < r->accept->latest) {
		max_latest = r->accept->latest;
		max_latest_c = r->accept->myid;
	    }
	}

	attempt->nvarg.latest = max_latest;
	attempt->nvarg.view.vid = attempt->proposed_vid;
	attempt->nvarg.view.backups.setsize(attempt->vcaccepts.size() - 1);
	for (uint32_t i = 0, j = 0; i < attempt->vcaccepts.size(); i++) {
	    ref<view_change_res> r = attempt->vcaccepts[i];
	    if (r->accept->myid == max_latest_c)
		attempt->nvarg.view.primary = r->accept->myid;
	    else
		attempt->nvarg.view.backups[j++] = r->accept->myid;
	}
    }

    void view_change_issue_newview(ptr<view_change_attempt> attempt) {
	vec<cohort_t> ul = attempt->underlings.members();
	for (uint32_t i = 0; i < ul.size(); i++) {
	    cohort_t u = ul[i];
	    ref<new_view_res> resp = New refcounted<new_view_res>();
	    sockaddr_in sin = netaddr2sockaddr(u.addr);
	    c_->call(REP_NEW_VIEW, &attempt->nvarg, resp,
		     wrap(mkref(this), &cohort::view_change_newview_cb, attempt, u, resp),
		     0, 0, 0, 0, 0, (sockaddr *) &sin);
	}
    }

    void view_change_check_prepare(ptr<view_change_attempt> attempt) {
	if (attempt->aborted || attempt->prepare_done.size() != attempt->underlings.size())
	    return;

	if (!attempt->prepare_accepted.contains_majority_of(*view2mids(view_))) {
	    attempt->aborted = true;
	    return;
	}

	if (attempt->newview) {
	    if (attempt->merged_vid < attempt->newview->vid) {
		/*
		 * Send out the REP_VIEW_CHANGE RPC again to any new members
		 * in attempt->newview that we didn't already contact.
		 * Must receive majority in current view (already got it)
		 * and majority in attempt->newview.  This will keep
		 * looping if we get another newview in the process.
		 */
		view_change_arg arg;
		arg.oldview = view_;
		arg.newvid = attempt->proposed_vid;

		vec<cohort_t> ul = view2cohorts(*attempt->newview)->members();
		for (uint32_t i = 0; i < ul.size(); i++) {
		    cohort_t u = ul[i];
		    if (attempt->underlings.insert(u)) {
			ref<view_change_res> resp = New refcounted<view_change_res>();
			sockaddr_in sin = netaddr2sockaddr(u.addr);
			c_->timedcall(timeout_vc_prepare, REP_VIEW_CHANGE, &arg, resp,
				      wrap(mkref(this), &cohort::view_change_prepare_cb,
					   vcattempt_, u, resp),
				      0, 0, 0, 0, 0, (sockaddr *) &sin);
		    }
		}

		attempt->merged_vid = attempt->newview->vid;
		view_change_check_prepare(attempt);
		return;
	    }

	    if (!attempt->prepare_accepted.contains_majority_of(*view2mids(*attempt->newview))) {
		attempt->aborted = true;
		return;
	    }
	}

	view_change_choose_view(attempt);
	view_change_issue_newview(attempt);
    }
    /* cs244b: end cut for lab */

    void view_change_prepare_cb(ptr<view_change_attempt> attempt, cohort_t c,
				ref<view_change_res> resp, clnt_stat stat) {
	if (attempt->aborted)
	    return;

	/* cs244b: start cut for lab */
	attempt->prepare_done.insert(c.mid);

	if (!stat && !attempt->aborted) {
	    if (!resp->accepted) {
		attempt->aborted = true;
		if (proposed_vid_ < resp->reject->newvid)
		    proposed_vid_ = resp->reject->newvid;
	    } else if (attempt->prepare_accepted.insert(c.mid)) {
		attempt->vcaccepts.push_back(resp);
		if (resp->accept->newview) {
		    if (!attempt->newview)
			attempt->newview = New refcounted<view_t>(*resp->accept->newview);
		    if (attempt->newview->vid < resp->accept->newview->vid)
			*attempt->newview = *resp->accept->newview;
		}
	    }
	}
        if (stat != RPC_SUCCESS) {
            warn << "error " << stat << " in view_change_prepare_cb:" << c.mid << "\n";
        }

	view_change_check_prepare(attempt);
	return;
	/* cs244b: end cut for lab */
	fatal << "Insert your code here.\n";
    }

    void view_change_initiate(vec<cohort_t> joiners) {
	if (vcattempt_ && !vcattempt_->aborted)
	    return;

	if (!is_view_member(self_.mid))
	    return;

	ptr<view_change_attempt> attempt = New refcounted<view_change_attempt>();
	attempt->aborted = false;
	/* cs244b: start cut for lab */
	memset(&attempt->merged_vid, 0, sizeof(attempt->merged_vid));
	/* cs244b: end cut for lab */

	vcattempt_ = attempt;
	mode_ = VC_VIEWCHANGE;

	attempt->underlings = *view2cohorts(view_);
	for (uint32_t i = 0; i < joiners.size(); i++)
	    attempt->underlings.insert(joiners[i]);

        warn << GetTimeMs() << ":" << "joiners:" << joiners.size()<< "\n";
	viewid_t maxview = view_.vid;
	if (maxview < proposed_vid_)
	    maxview = proposed_vid_;

	attempt->proposed_vid.counter = maxview.counter + 1;
	attempt->proposed_vid.manager = self_.mid;

	view_change_arg arg;
	arg.oldview = view_;
	arg.newvid = attempt->proposed_vid;

	vec<cohort_t> ul = attempt->underlings.members();
	for (uint32_t i = 0; i < ul.size(); i++) {
	    cohort_t u = ul[i];
	    ref<view_change_res> resp = New refcounted<view_change_res>();
	    sockaddr_in sin = netaddr2sockaddr(u.addr);
            /* begin jbl nasty hack */
            std::list<mid_t>::iterator it;
            bool in_active_servers = false;
            for (it=active_servers_.begin(); it != active_servers_.end(); it++) {
                if (*it == u.mid) {
                    in_active_servers = true;
                    break;
                }
            }
            bool in_joiners = false;
            for (uint32_t i = 0; i < joiners.size(); i++) {
                if (joiners[i].mid == u.mid) {
                    in_joiners = true;
                    break;
                }
            }
            if (in_joiners || in_active_servers) {
                c_->timedcall(timeout_vc_prepare, REP_VIEW_CHANGE, &arg, resp,
                              wrap(mkref(this), &cohort::view_change_prepare_cb,
                                   attempt, u, resp),
                              0, 0, 0, 0, 0, (sockaddr *) &sin);
            } else {
                view_change_prepare_cb(attempt, u, resp, RPC_TIMEDOUT);
                // Make the timeout call?
            }
            /* end jbl nasty hack */
	}
    }

    viewstamp_t vs_next(viewstamp_t vs) {
	viewstamp_jump_entry *e = jumps_[vs];
	if (e && e->prev == vs)
	    return e->next;
	return vs + 1;
    }

    void vs_jumps_update(request_log_entry *rle) {
	if (rle->vs.ts != 0)
	    return;

	str s(rle->arg.request.base(), rle->arg.request.size());
	view_form_t f;
	if (!str2xdr(f, s))
	    fatal << "vs_jumps_update: cannot unmarshal view_form_t\n";

	viewstamp_jump_entry *e = jumps_[f.prev];
	if (!e) {
	    e = New viewstamp_jump_entry();
	    e->prev = f.prev;
	    e->next = rle->vs;
	    jumps_.insert(e);
	}

	if (e->next < rle->vs)
	    e->next = rle->vs;
    }

    void view_is_down() {
	warn << GetTimeMs() << ":"<< "Current view is degraded, attempting a view change..\n";
	vec<cohort_t> joiners;
	view_change_initiate(joiners);
    }

    void execute_done(request_log_entry *rle, str reply) {
	rle->state = RQ_EXECUTED;

	reply_cache_entry *rce = replycache_[rle->arg.client];
	if (!rce || rce->client != rle->arg.client) {
	    rce = New reply_cache_entry();
	    rce->client = rle->arg.client;
	    replycache_.insert(rce);
	}

	rce->rid = rle->arg.rid;
	rce->rle = rle;
	rce->reply = reply;

	if (rle->sbp) {
	    execute_res res;
	    memset(&res, 0, sizeof(res));
	    res.set_ok(true);
	    res.reply->setsize(reply.len());
	    memcpy(res.reply->base(), reply.cstr(), reply.len());
	    rle->sbp->reply(&res);
	    rle->sbp = 0;
	}
    }

    void on_falcon_connect(char* domain, char* host, mid_t m, int fd) {
    	pfd_server_handshake hs;
	strcpy(hs.app_handle, "pmp");
	strcpy(hs.host_name, domain);
	strcpy(hs.vmm_name, host);
	strcpy(hs.switch_name, "router");
	if (sizeof(hs) != send(fd, &hs, sizeof(hs), 0)) 
            fatal << "error sending to failure detector\n";
	mid2fdfd_[m] = fd;
	fdcb(mid2fdfd_[m], selread, wrap(mkref(this), &cohort::handle_failed, domain, host, m));
    }
    	
    void falcon_connect(const char* _domain, const char* _host, mid_t m) {
        char *domain = (char *) malloc(strlen(_domain) + 1);
        strcpy(domain, _domain);
        char *host = (char *) malloc(strlen(_host) + 1);
        strcpy(host, _host);
	tcpconnect("127.0.0.1", 9090, wrap(mkref(this), &cohort::on_falcon_connect, domain, host, m));
    }

    void handle_failed(char* domain, char* host, mid_t mid) {
	// Uncontrollably lame
    	assert(mid2fdfd_[mid] > 0);
        warn << GetTimeMs() << ":"<< "Got a failure " << domain << ":" << host << "\n";
        fdcb(mid2fdfd_[mid], selread, 0);
    	close(mid2fdfd_[mid]);
        free(domain);
        free(host);
        // Only initiate on primary failure?
        active_servers_.remove(mid);
        if (*(active_servers_.begin()) == self_.mid) {
            view_is_down();
            ping_timeout_cb_ =
                delaycb(timeout_view_change,
                        wrap(mkref(this), &cohort::view_is_down));
        }
    }

    void advance_executed() {
	for (viewstamp_t vs = vs_next(executed_); vs <= committed_; vs = vs_next(vs)) {
	    request_log_entry *rle = log_[vs];
	    if (!rle || rle->vs != vs)
		break;

	    str buf(rle->arg.request.base(), rle->arg.request.size());
	    if (rle->vs.ts) {
		(exec_)(buf, wrap(mkref(this), &cohort::execute_done, rle));
	    } else {
		view_form_t f;
		if (!str2xdr(f, buf))
		    fatal << "advance_executed: cannot unmarshal view_form_t\n";

		view_ = f.view;
		proposed_vid_ = f.view.vid;
		valid_view_ = true;
		accepted_view_ = 0;
		if (vcattempt_)
		    vcattempt_->aborted = true;

		mode_ = VC_ACTIVE;

                if (ping_timeout_cb_) {
                    timecb_remove(ping_timeout_cb_);
                    ping_timeout_cb_ = 0;
                }

		warn << GetTimeMs() << ":"<< "New view " << view_.vid << "\n";
		if (view_.primary.mid == self_.mid) {
		    warn << GetTimeMs() << ":"<< "This cohort is king in " << view_.vid << "\n";
		}
                active_servers_.clear();
		for (size_t i = 0; i < view_.backups.size(); i++) {
			mid_t m = view_.backups[i].mid;
                        active_servers_.push_back(m);
			if (m != self_.mid && mid2fdfd_[m] == 0) {
				warn << GetTimeMs() << ":"<< "There goes a falcon:" << view_.backups[i].name << " " << view_.backups[i].hypervisor << "\n";
				warn << GetTimeMs() << ":"<< m << " is who it's after\n";
				mid2fdfd_[m] = -1; // Let's us know we're already getting the connection
				falcon_connect(view_.backups[i].name, view_.backups[i].hypervisor, m);
			}
		}
		mid_t m = view_.primary.mid;
                active_servers_.push_back(m);
		if (m != self_.mid && mid2fdfd_[m] == 0) {
			warn << GetTimeMs() << ":"<< "There goes a falcon:" << view_.primary.name << " " << view_.primary.hypervisor << "\n";
			warn << GetTimeMs() << ":"<< m << " is who it's after\n";
			mid2fdfd_[m] = -1; // Let's us know we're already getting the connection
			falcon_connect(view_.primary.name, view_.primary.hypervisor, m);
		}

                active_servers_.sort();

	    }

	    executed_ = vs;
	    rle->state = RQ_EXECUTING;
	}
    }

    void advance_committed() {
	for (viewstamp_t vs = vs_next(committed_); vs <= latest_; vs = vs_next(vs)) {
	    request_log_entry *rle = log_[vs];
	    assert(rle && rle->vs == vs);

	    if (!rle->ack->received.contains_majority_of(rle->ack->expected)) {
		/*
		 * XXX we may want to timestamp RLEs and resend
		 * RES_REPLICATEs after a while..
		 */
		break;
	    }

	    committed_ = vs;
	    rle->ack = 0;
	    rle->state = RQ_COMMITTED;
	}

	advance_executed();
    }

    void replicate_cb(mid_t backup_mid, ref<replicate_res> resp, clnt_stat stat) {
	if (stat) {
	    warn << GetTimeMs() << ":"<< "Error " << stat
		 << " trying to talk to backup cohort " << backup_mid << "\n";
	    return;
	}

	if (view_.primary.mid != self_.mid) {
	    warn << GetTimeMs() << ":"<< "Replicate reply received by a backup\n";
	    return;
	}

	assert(latest_.vid == committed_.vid);
	if (resp->vs <= committed_) {
	    /* We didn't really care about this guy anymore.. */
	    return;
	}

	if (latest_ < resp->vs)
	    fatal << "Protocol error: acknowledging beyond latest";

	if (!is_view_member(backup_mid)) {
	    warn << GetTimeMs() << ":"<< "Received REPLICATE ACK from a non-view-member\n";
	    return;
	}

	for (viewstamp_t vs = vs_next(committed_); vs <= resp->vs; vs = vs_next(vs)) {
	    request_log_entry *rle = log_[vs];
	    assert(rle && rle->vs == vs);
	    if (rle->ack)
		rle->ack->received.insert(backup_mid);
	}

	advance_committed();
    }

    struct pull_log_state {
	ptr<ackset<viewstamp_t> > ack;
	viewid_t bottomview;
	cohort_t src;
	cbb donecb;
	bool ok;
	bool bottomdone;

	pull_log_state(cbb cb) : donecb(cb), bottomdone(false) {}
    };

    void fetchlog_check(ptr<pull_log_state> pls) {
	advance_committed();

	if (vs_next(executed_).vid < pls->bottomview) {
	    viewstamp_t xs;
	    xs.vid = pls->bottomview;
	    xs.ts = 0;

	    request_log_entry *rle = log_[xs];
	    if (rle && rle->vs == xs) {
		str s(rle->arg.request.base(), rle->arg.request.size());
		view_form_t f;
		if (!str2xdr(f, s))
		    fatal << "fetchlog_cb: cannot unmarshal view_form_t\n";
		pls->bottomview = f.prev.vid;
		pull_upto(pls, f.prev);
	    }
	} else {
	    pls->bottomdone = true;
	}

	if (!pls->ok || pls->ack->expected.size() == pls->ack->received.size()) {
	    (*pls->donecb)(pls->ok && pls->bottomdone);
	    pls->donecb = cbb_null;
	}
    }

    void fetchlog_cb(ptr<pull_log_state> pls, viewstamp_t vs,
		     ref<execute_arg> resp, clnt_stat stat) {
	request_log_entry *rle = log_[vs];
	if (!rle || rle->vs != vs) {
	    if (stat) {
		/* XXX should we ask again? */
		pls->ok = false;
		fetchlog_check(pls);
		warn << GetTimeMs() << ":"<< "fetchlog_cb: " << stat << "\n";
		return;
	    } else {
		rle = New request_log_entry();
		rle->vs = vs;
		rle->arg = *resp;
		rle->ack = 0;
		rle->sbp = 0;
		vs_jumps_update(rle);
		log_.insert(rle);
	    }
	}

	pls->ack->received.insert(vs);
	fetchlog_check(pls);
    }

    void pull_upto(ptr<pull_log_state> pls, viewstamp_t latest) {
	/*
	 * Queue up calls asking for any missing segments up to
	 * and including latest, from cohort pls->src.
	 *
	 * XXX would be nice to keep track of what we have
	 * already requested to avoid flooding pls->src with
	 * requests for the same log entries..
	 */
	viewstamp_t fetchbase;
	if (latest.vid < executed_.vid) {
	    return;
	} else if (latest.vid == executed_.vid) {
	    fetchbase = executed_ + 1;
	} else {
	    fetchbase.vid = latest.vid;
	    fetchbase.ts = 0;
	}

	for (viewstamp_t vs = fetchbase; vs <= latest; ++vs) {
	    request_log_entry *rle = log_[vs];
	    if (!rle || rle->vs != vs) {
		pls->ack->expected.insert(vs);

		ref<execute_arg> resp = New refcounted<execute_arg>();
		sockaddr_in sin = netaddr2sockaddr(pls->src.addr);
		c_->call(REP_FETCHLOG, &vs, resp,
			 wrap(mkref(this), &cohort::fetchlog_cb, pls, vs, resp),
			 0, 0, 0, 0, 0, (sockaddr *) &sin);
	    }
	}
    }

    void pull_log(cohort_t src, viewstamp_t latest, cbb donecb) {
	ptr<pull_log_state> pls = New refcounted<pull_log_state>(donecb);
	pls->src = src;
	pls->ack = New refcounted<ackset<viewstamp_t> >();
	pls->ok = true;
	pls->bottomview = latest.vid;
	pull_upto(pls, latest);
	fetchlog_check(pls);
    }

    static void replicate_pulldone(svccb *sbp, viewstamp_t vs, bool ok) {
	if (!ok) {
	    sbp->reject(SYSTEM_ERR);
	    return;
	}

	replicate_res res;
	res.vs = vs;
	sbp->reply(&res);
    }

    static void new_view_pulldone(svccb *sbp, bool ok) {
	new_view_res res;
	res.accepted = ok;
	sbp->reply(&res);
    }

    void dispatch_bcast(svccb *sbp) {
	switch (sbp->proc()) {
	case REP_BCAST_NULL:
	    sbp->reply(0);
	    break;

	case REP_BCAST_VIEWINFO: {
	    if (!valid_view_) {
		sbp->ignore();
		return;
	    }

	    execute_viewinfo res;
	    memset(&res, 0, sizeof(res));
	    res.vid = view_.vid;
	    res.primary = view_.primary.addr;
            res.primary_c = view_.primary;
	    sbp->reply(&res);
	    break;
        }

	case REP_BCAST_LETMEIN: {
	    cohort_t *argp = sbp->Xtmpl getarg<cohort_t> ();
	    vec<cohort_t> joiners;
	    joiners.push_back(*argp);

	    view_change_initiate(joiners);
	    sbp->reply(0);
	    break;
	}

	default:
	    sbp->reject(PROC_UNAVAIL);
	}
    }

    void dispatch(svccb *sbp) {
	switch (sbp->proc()) {
	case REP_NULL:
	    sbp->reply(0);
	    break;

	case REP_VIEWINFO: {
	    if (mode_ != VC_ACTIVE) {
		sbp->ignore();
		return;
	    }

	    execute_viewinfo res;
	    memset(&res, 0, sizeof(res));
	    res.vid = view_.vid;
	    res.primary = view_.primary.addr;
	    sbp->reply(&res);
	    break;
	}

	case REP_EXECUTE: {
	    if (mode_ != VC_ACTIVE) {
		sbp->ignore();
		return;
	    }

	    execute_arg *argp = sbp->Xtmpl getarg<execute_arg> ();
	    reply_cache_entry *rce = replycache_[argp->client];
	    if (rce && rce->client == argp->client && rce->rid == argp->rid) {
		if (rce->rle->state == RQ_EXECUTED) {
		    warn << GetTimeMs() << ":"<< "Reply cache match: completed reqest\n";
		    execute_res res;
		    memset(&res, 0, sizeof(res));
		    res.set_ok(true);
		    *res.reply = rce->reply;
		    sbp->reply(&res);
		    return;
		} else {
		    if (rce->rle->sbp) {
			rce->rle->sbp->ignore();
			rce->rle->sbp = 0;
		    }

		    /*
		     * If it was in a different view, and didn't get committed
		     * yet, fall through and re-issue it.  Shouldn't happen.
		     */
		    if (!(rce->rle->state == RQ_LOGGED && rce->rle->vs.vid < view_.vid)) {
			warn << GetTimeMs() << ":"<< "Reply cache match: pending request\n";
			rce->rle->sbp = sbp;
			return;
		    }
		}
	    }

	    if (argp->vid != view_.vid || self_.mid != view_.primary.mid) {
		execute_res res;
		memset(&res, 0, sizeof(res));
		res.set_ok(false);
		res.viewinfo->vid = view_.vid;
		res.viewinfo->primary = view_.primary.addr;
		sbp->reply(&res);
		return;
	    }

	    /* This primary cohort will try to accept this request. */
	    ++latest_;

	    replicate_arg ra;
	    ra.vs = latest_;
	    ra.arg = *argp;
	    ra.committed = committed_;

	    request_log_entry *rle = New request_log_entry();
	    rle->vs = ra.vs;
	    rle->arg = ra.arg;
	    rle->ack = New refcounted<ackset<mid_t> >();
	    rle->sbp = sbp;
	    vs_jumps_update(rle);
	    log_.insert(rle);

	    if (!rce || rce->client != argp->client) {
		rce = New reply_cache_entry();
		rce->client = rle->arg.client;
		replycache_.insert(rce);
	    }

	    rce->rid = argp->rid;
	    rce->rle = rle;

	    rle->ack->expected.insert(self_.mid);
	    rle->ack->received.insert(self_.mid);

	    for (uint32_t i = 0; i < view_.backups.size(); i++) {
		cohort_t b = view_.backups[i];

		ref<replicate_res> resp = New refcounted<replicate_res>();
		sockaddr_in sin = netaddr2sockaddr(b.addr);
		rle->ack->expected.insert(b.mid);
		c_->call(REP_REPLICATE, &ra, resp,
			 wrap(mkref(this), &cohort::replicate_cb, b.mid, resp),
			 0, 0, 0, 0, 0, (sockaddr *) &sin);
	    }

	    advance_committed();
	    break;
	}

	case REP_REPLICATE: {
	    replicate_arg *argp = sbp->Xtmpl getarg<replicate_arg> ();

	    cohort_t primary;
	    if (argp->vs.ts != 0) {
		/* These checks don't apply to view formation */
		if (mode_ != VC_ACTIVE) {
		    warn << GetTimeMs() << ":"<< "Ignoring REPLICATE request while not VC_ACTIVE\n";
		    sbp->ignore();
		    return;
		}

		if (view_.primary.mid == self_.mid) {
		    warn << GetTimeMs() << ":"<< "REP_REPLICATE received by primary\n";
		    sbp->reject(SYSTEM_ERR);
		    return;
		}

		if (!is_view_member(self_.mid)) {
		    warn << GetTimeMs() << ":"<< "REP_REPLICATE received by non-view-member\n";
		    sbp->reject(SYSTEM_ERR);
		    return;
		}

		primary = view_.primary;
	    } else {
		str s(argp->arg.request.base(), argp->arg.request.size());
		view_form_t f;
		if (!str2xdr(f, s))
		    fatal << "vs_jumps_update: cannot unmarshal view_form_t\n";

		primary = f.view.primary;
	    }

	    request_log_entry *rle = log_[argp->vs];
	    if (committed_ < argp->vs && (!rle || rle->vs != argp->vs)) {
		rle = New request_log_entry();
		rle->vs = argp->vs;
		rle->arg = argp->arg;
		rle->ack = 0;
		rle->sbp = 0;
		vs_jumps_update(rle);
		log_.insert(rle);
	    }

	    if (committed_ < argp->committed)
		committed_ = argp->committed;
	    advance_executed();

	    pull_log(primary, argp->vs,
		     wrap(&cohort::replicate_pulldone, sbp, argp->vs));
	    break;
	}

	case REP_FETCHLOG: {
	    viewstamp_t *argp = sbp->Xtmpl getarg<viewstamp_t> ();
	    request_log_entry *rle = log_[*argp];
	    if (!rle || rle->vs != *argp) {
		warn << GetTimeMs() << ":"<< "REP_FETCHLOG for missing log entry\n";
		sbp->reject(SYSTEM_ERR);
		return;
	    }
	    sbp->reply(&rle->arg);
	    break;
	}

	case REP_LETMEIN: {
	    cohort_t *argp = sbp->Xtmpl getarg<cohort_t> ();
	    vec<cohort_t> joiners;
	    joiners.push_back(*argp);

	    view_change_initiate(joiners);
	    sbp->reply(0);
	    break;
	}

	case REP_VIEW_CHANGE: {
	    view_change_arg *argp = sbp->Xtmpl getarg<view_change_arg> ();
	    view_change_res res;

	    if ((valid_view_ && argp->oldview.vid < view_.vid) ||
		(argp->newvid < proposed_vid_))
	    {
		res.set_accepted(false);
		if (valid_view_)
		    *res.reject->oldview.alloc() = view_;
		else
		    res.reject->oldview.clear();
		res.reject->newvid = proposed_vid_;
		sbp->reply(&res);
		return;
	    }

	    mode_ = VC_VIEWCHANGE;

	    proposed_vid_ = argp->newvid;
	    res.set_accepted(true);
	    res.accept->myid = self_;
	    res.accept->include_me = true;

	    /* Find the latest consecutive log entry */
	    res.accept->latest = executed_;
	    for (viewstamp_t vs = vs_next(executed_); ; vs = vs_next(vs)) {
		request_log_entry *rle = log_[vs];
		if (!rle || rle->vs != vs)
		    break;
		res.accept->latest = vs;
	    }

	    res.accept->newview.clear();
	    if (accepted_view_)
		*res.accept->newview.alloc() = *accepted_view_;
	    if (vcattempt_ && vcattempt_->proposed_vid != argp->newvid)
		vcattempt_->aborted = true;
	    sbp->reply(&res);
	    break;
	}

	case REP_NEW_VIEW: {
	    new_view_arg *argp = sbp->Xtmpl getarg<new_view_arg> ();

	    if ((valid_view_ && argp->view.vid < view_.vid) || argp->view.vid < proposed_vid_) {
		new_view_res res;
		res.accepted = false;
		sbp->reply(&res);
		return;
	    }

	    mode_ = VC_VIEWCHANGE;

	    if (vcattempt_ && vcattempt_->proposed_vid != argp->view.vid)
		vcattempt_->aborted = true;

	    accepted_view_ = New refcounted<view_t> (argp->view);
	    pull_log(argp->view.primary, argp->latest,
		     wrap(&cohort::new_view_pulldone, sbp));
	    break;
	}

	case REP_INIT_VIEW: {
	    view_form_t *argp = sbp->Xtmpl getarg<view_form_t> ();
	    if (!valid_view_ || view_.vid < argp->view.vid)
		form_view(*argp);
	    sbp->reply(0);
	    break;
	}

	case REP_PING: {
	    assert(false);
	    ping_arg *argp = sbp->Xtmpl getarg<ping_arg> ();
	    ping_res res;
	    res.ok = valid_view_ && argp->vid == view_.vid;
	    if (res.ok)
		ping_incoming_.insert(argp->myid);
	    sbp->reply(&res);
	    break;
	}

	/* cs244b: start cut for lab */
	case REP_GET_VIEW:
	    if (valid_view_)
		sbp->reply(&view_);
	    else
		sbp->ignore();
	    break;
	/* cs244b: end cut for lab */

	default:
	    sbp->reject(PROC_UNAVAIL);
	}
    }

    ptr<asrv> ucast_srv_, bcast_srv_;
    ptr<aclnt> c_, bc_;

    itree<viewstamp_t, request_log_entry,
	  &request_log_entry::vs, &request_log_entry::link> log_;
    itree<viewstamp_t, viewstamp_jump_entry,
	  &viewstamp_jump_entry::prev, &viewstamp_jump_entry::link> jumps_;
    itree<mid_t, reply_cache_entry,
	  &reply_cache_entry::client, &reply_cache_entry::link> replycache_;
    cohort_t self_;
    execute_t exec_;

    enum { VC_ACTIVE, VC_BOOTSTRAP, VC_VIEWCHANGE } mode_;
    bool valid_view_;
    view_t view_;
    viewid_t proposed_vid_;
    ptr<view_t> accepted_view_;
    ptr<view_change_attempt> vcattempt_;

    viewstamp_t latest_;	/* timestamp of latest request from this primary */
    viewstamp_t committed_;	/* this and everything below can be executed */
    viewstamp_t executed_;	/* this and everything below sent to execute() */

    set<mid_t> ping_incoming_;
    timecb_t *ping_timeout_cb_;
};

static void
go(uint16_t group_port, sockaddr_in join, const char* name, const char* hypervisor, int exec_port)
{
    ptr<exec_server> e = New refcounted<exec_server>("127.0.0.1", exec_port);
    static ptr<cohort> c =
	New refcounted<cohort>(group_port,
			       wrap(e, &exec_server::execute), join, name, hypervisor);
}

void
spin() {
    for(;;);
}

void
recurse() {
    delaycb(0, 0, wrap(recurse));
}

void
saboteur(const char* failure) {
    if (!strcmp("segfault", failure)) {
        int *x = NULL;
        *x = 11;
    } else if (!strcmp("badloop", failure)) {
        delaycb(0,0, wrap(spin));
        for (;;);
    } else if (!strcmp("livelock", failure)) {
        delaycb(0,0, wrap(recurse));
    }
}

#ifndef FALCON_INC_RATE_NS
#define FALCON_INC_RATE_NS 1000000
#endif

void
inc_count() {
    the_count_++;
    delaycb(0, FALCON_INC_RATE_NS, wrap(inc_count));
    return;
}

uint32_t
spy() {
    // this is dirrrty. 
    static uint32_t my_count = 0;
    my_count = the_count_;
    usleep(FALCON_INC_RATE_NS/100);
    bool alive = my_count != the_count_;
    return (alive) ? PROC_OBS_ALIVE : PROC_OBS_DEAD;
}

int
main(int ac, char **av)
{
    if (ac != 5)
	fatal << "Usage: " << av[0] << " backend-server-path group-udp-port join-cohort:port hypervisor_hostname\n";

    uint16_t group_port = atoi(av[2]);

    char *join_serv = av[3];
    char *colon = strchr(join_serv, ':');
    
    char hostname[32];
    gethostname(hostname, 32);
    char *hypervisor = av[4];
    if (!colon)
	fatal << "Missing colon and port number in join server address\n";

    *colon = '\0';

    sockaddr_in join;
    memset(&join, 0, sizeof(join));
    join.sin_family = AF_INET;
    join.sin_port = htons(atoi(colon + 1));
    if (!inet_aton(join_serv, &join.sin_addr))
	fatal << "Cannot parse IP address of cohort to join\n";
    if (join.sin_addr.s_addr == 0)
	join.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    delaycb(0, FALCON_INC_RATE_NS, wrap(inc_count));
    SetSabotage(&saboteur);
    SetSpy(&spy, "pmp");
    str exec_pn(av[1]);
    launch_backend(exec_pn, wrap(go, group_port, join, hostname, hypervisor));
    amain();
}
