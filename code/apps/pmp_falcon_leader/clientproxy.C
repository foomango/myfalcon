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
#include "messages.h"

#include <map>

#ifndef VIEW_POLL_PERIOD_S
#define VIEW_POLL_PERIOD_S 1
#endif

enum {
    timeout_execute = 3,
};

struct counter {
    int c;
};

typedef callback<void, str>::ref request_cb;

void ignore_str(str s) {
    return;
}

struct falcon_info {
    falcon_info (request_cb cb, ptr<execute_arg> req, callbase* rpc_cb) :
        cb_(cb), req_(req), rpc_cb_(rpc_cb) {}
    request_cb          cb_;
    ptr<execute_arg>    req_;
    callbase*           rpc_cb_;
};

std::map<rid_t,falcon_info*> falcon_map_;

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
        primary_mid_ = 0;
    }

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
            primary_mid_ = vi->primary_c.mid;
            falcon_connect(vi->primary_c.name, vi->primary_c.hypervisor, primary_mid_);
	} else {
            warn << "Ignoring " << viewinfo_->vid << " " << vi->vid << "\n";
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
            falcon_info* info = falcon_map_[req->rid];
            falcon_map_.erase(req->rid);
            delete info;
	    (*cb)(str(resp->reply->base(), resp->reply->size()));
	    return;
	}

	update_view(resp->viewinfo); 
        process_request(req, cb);
    }

    void check_new_primary(mid_t old_primary_mid, ptr<execute_arg> req, request_cb cb) {
        if (old_primary_mid == primary_mid_) {
            refresh_view(req, cb);
            delaycb(VIEW_POLL_PERIOD_S, wrap(mkref(this), &rep_client::check_new_primary, old_primary_mid, req, cb));
        }
        return;
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
	callbase * rpc_cb =
        c_->timedcall(timeout_execute, REP_EXECUTE, req, resp,
		      wrap(mkref(this), &rep_client::rep_execute_cb, req, cb, resp),
		      0, 0, 0, 0, 0, (sockaddr *) &sin);
        falcon_info *f = new falcon_info(cb, req, rpc_cb);
        falcon_map_[req->rid] = f;
    }

    void on_falcon_connect(char* domain, char* host, mid_t m, int fd) {
        pfd_server_handshake hs;
        strcpy(hs.app_handle, "pmp");
        strcpy(hs.host_name, domain);
        strcpy(hs.vmm_name, host);
        strcpy(hs.switch_name, "router");
        if (sizeof(hs) != send(fd, &hs, sizeof(hs), 0))
            fatal << "error sending to failure detector\n";
        warn << "There's a falcon after " << domain << ":" << host << "\n";
        fdcb(fd, selread, wrap(mkref(this), &rep_client::handle_failed, domain, host, m, fd));
    }

    void falcon_connect(const char* _domain, const char* _host, mid_t m) {
        char *domain = (char *) malloc(strlen(_domain) + 1);
        strcpy(domain, _domain);
        char *host = (char *) malloc(strlen(_host) + 1);
        strcpy(host, _host);
        tcpconnect("127.0.0.1", 9090, wrap(mkref(this), &rep_client::on_falcon_connect, domain, host, m));
    }

    void handle_failed(char* domain, char* host, mid_t mid, int fd) {
        warn << "Got failure " << domain << ":" << host << "\n";
        free(domain);
        free(host);
        close(fd);
        fdcb(fd, selread, 0);
        if (mid != primary_mid_) {
            return;
        }
        std::map<rid_t,falcon_info*>::iterator it;
        for (it = falcon_map_.begin(); it != falcon_map_.end(); ++it) {
            falcon_info* info = (it->second);
            info->rpc_cb_->cancel();
            warn << "Refreshing view\n";
            delaycb(0, 100 * 1000 * 1000, wrap(mkref(this),
                    &rep_client::check_new_primary, mid, info->req_, info->cb_));
        }
    }


    ptr<aclnt> c_, bc_;
    mid_t client_;
    rid_t rid_;
    mid_t primary_mid_;

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
