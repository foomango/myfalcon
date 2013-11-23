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

#include <arpc.h>
#include <dns.h>
#include "rep.h"
#include "util.h"
#include "bcast.h"

enum {
    timeout_execute = 3,
};

struct counter {
    int c;
};

class rep_client : public virtual refcount {
 public:
    rep_client(uint16_t bcast_port) {
	memset(&bootstrap_, 0, sizeof(bootstrap_));
	bootstrap_.sin_family = AF_INET;
	bootstrap_.sin_port = htons(bcast_port);
	bootstrap_.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	int ucast_fd = inetsocket(SOCK_DGRAM, 0, 0);
	if (ucast_fd < 0)
	    fatal << "inetsocket: " << strerror(errno) << "\n";
	make_async(ucast_fd);
	close_on_exec(ucast_fd);
	c_ = aclnt::alloc(axprt_dgram::alloc(ucast_fd), rep_prog_1, 0,
			  callbase_alloc<rpccb_unreliable_sockaddrin>);

	int bcast_fd = bcast_info_t::bind_bcast_sock();
	if (bcast_fd < 0)
	    fatal << "bind_bcast_sock: " << strerror(errno) << "\n";
	make_async(bcast_fd);
	close_on_exec(bcast_fd);
	bc_ = aclnt::alloc(axprt_dgram::alloc(bcast_fd), rep_bcast_prog_1, 0,
			   callbase_alloc<rpccb_bcast>);

	client_ = unique_process_id();
    }

    typedef callback<void, str>::ref request_cb;
    void issue_request(str req, request_cb cb) {
	ptr<execute_arg> arg = New refcounted<execute_arg>();

	arg->client = client_;
	arg->rid = rid_++;
	arg->request.setsize(req.len());
	memcpy(arg->request.base(), req.cstr(), req.len());

	if (viewinfo_)
	    process_request(arg, cb);
	else
	    refresh_view(arg, cb);
    }

 private:
    void rep_viewinfo_cb(ptr<execute_arg> req, request_cb cb, ref<execute_viewinfo> resp,
			 ptr<counter> retried, clnt_stat stat) {
	if (!stat)
	    update_view(resp);

	if (!retried->c) {
	    if (!stat) {
		process_request(req, cb);
		retried->c = 1;
	    }

	    if (stat == RPC_TIMEDOUT) {
		refresh_view(req, cb);
		retried->c = 1;
	    }
	}
    }

    void update_view(execute_viewinfo *vi) {
	warn << "Got a VIEWINFO response for view " << vi->vid << "\n";
	if (!viewinfo_ || viewinfo_->vid < vi->vid) {
	    sockaddr_in sin = netaddr2sockaddr(vi->primary);
	    warn << "Using new VIEWINFO as latest, primary "
		 << inet_ntoa(sin.sin_addr) << ":" << ntohs(sin.sin_port) << "\n";
	    viewinfo_ = New refcounted<execute_viewinfo>(*vi);
	}
    }

    void rep_execute_cb(ptr<execute_arg> req, request_cb cb,
			ref<execute_res> resp, clnt_stat stat) {
	if (stat) {
	    warn << "Error executing request: " << stat << "\n";
	    refresh_view(req, cb);
	    return;
	}

	if (resp->ok) {
	    (*cb)(str(resp->reply->base(), resp->reply->size()));
	    return;
	}

	update_view(resp->viewinfo);
	process_request(req, cb);
    }

    void refresh_view(ptr<execute_arg> req, request_cb cb) {
	ref<execute_viewinfo> resp = New refcounted<execute_viewinfo>();
	ptr<counter> retried = New refcounted<counter>();
	retried->c = 0;
	bc_->call(REP_BCAST_VIEWINFO, 0, resp,
		  wrap(mkref(this), &rep_client::rep_viewinfo_cb, req, cb, resp, retried),
		  0, 0, 0, 0, 0, (sockaddr *) &bootstrap_);
    }

    void process_request(ptr<execute_arg> req, request_cb cb) {
	if (!viewinfo_) {
	    refresh_view(req, cb);
	    return;
	}

	req->vid = viewinfo_->vid;
	ref<execute_res> resp = New refcounted<execute_res>();
	sockaddr_in sin = netaddr2sockaddr(viewinfo_->primary);
	c_->timedcall(timeout_execute, REP_EXECUTE, req, resp,
		      wrap(mkref(this), &rep_client::rep_execute_cb, req, cb, resp),
		      0, 0, 0, 0, 0, (sockaddr *) &sin);
    }

    ptr<aclnt> c_, bc_;
    mid_t client_;
    rid_t rid_;

    sockaddr_in bootstrap_;
    ptr<execute_viewinfo> viewinfo_;
};

class proxy_client : public virtual refcount {
 public:
    proxy_client(int fd, ptr<rep_client> rc) : dead_(false), rc_(rc) {
	xc_ = axprt_stream::alloc(fd);
	xc_->setrcb(wrap(mkref(this), &proxy_client::client_rcv));
    }

 private:
    void client_rcv(const char *pkt, ssize_t len, const sockaddr *addr) {
	if (pkt == 0) {
	    dead_ = true;
	    xc_->setrcb(0);
	} else {
	    str req(pkt, len);
	    rc_->issue_request(req, wrap(mkref(this), &proxy_client::send_reply));
	}
    }

    void send_reply(str resp) {
	if (!dead_)
	    xc_->send(resp.cstr(), resp.len(), 0);
    }

    bool dead_;
    ptr<axprt> xc_;
    ptr<rep_client> rc_;
};

static void
client_accept(int fd, ptr<rep_client> rc)
{
    sockaddr_in sin;
    socklen_t len = sizeof(sin);
    int s = accept(fd, (sockaddr *) &sin, &len);
    if (s < 0)
	return;

    vNew refcounted<proxy_client>(s, rc);
}

int
main(int ac, char **av)
{
    if (ac != 3)
	fatal << "Usage: " << av[0] << " listen-port group-udp-port\n";

    uint16_t lport = atoi(av[1]);
    int fd = inetsocket(SOCK_STREAM, lport);
    if (fd < 0)
	fatal << "Could not bind TCP port " << lport << "\n";

    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(fd, (struct sockaddr *) &sin, &len) < 0)
	fatal << "getsockname\n";

    warn << "clientproxy listening on port " << ntohs(sin.sin_port) << "\n";
    make_async(fd);
    listen(fd, 5);

    char *group_port = av[2];
    ptr<rep_client> rc = New refcounted<rep_client>(atoi(group_port));
    fdcb(fd, selread, wrap(&client_accept, fd, rc));

    amain();
}
