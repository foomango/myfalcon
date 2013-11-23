/* 
 * Copyright (C) 2011 David Mazieres (dm@uun.org)
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
#include "rep.h"
#include "util.h"
#include "bcast.h"

/* cs244b: start cut for lab */
class groupstat : public virtual refcount {
 public:
    groupstat(uint16_t bcast_port) {
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

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	sin.sin_port = htons(bcast_port);
	ref<execute_viewinfo> resp = New refcounted<execute_viewinfo>();
	bc_->call(REP_BCAST_VIEWINFO, 0, resp,
		  wrap(mkref(this), &groupstat::rep_viewinfo_cb, resp),
		  0, 0, 0, 0, 0, (sockaddr *) &sin);
    }

 private:
    void rep_viewinfo_cb(ref<execute_viewinfo> resp, clnt_stat stat) {
	if (stat) {
	    warn << "Error in REP_BCAST_VIEWINFO: " << stat << "\n";
	    exit(1);
	}

	sockaddr_in sin = netaddr2sockaddr(resp->primary);
	ref<view_t> resp2 = New refcounted<view_t>();
	c_->call(REP_GET_VIEW, 0, resp2,
		 wrap(mkref(this), &groupstat::rep_get_view_cb, resp2),
		 0, 0, 0, 0, 0, (sockaddr *) &sin);
    }

    void rep_get_view_cb(ref<view_t> resp, clnt_stat stat) {
	if (stat) {
	    warn << "Error in REP_GET_VIEW: " << stat << "\n";
	    exit(1);
	}

	warn << "Current view: " << resp->vid << "\n";
	sockaddr_in sin = netaddr2sockaddr(resp->primary.addr);
	warn << "Primary: " << resp->primary.mid << " at "
	     << inet_ntoa(sin.sin_addr) << ":" << resp->primary.addr.port << "\n";
	for (uint32_t i = 0; i < resp->backups.size(); i++) {
	    sin = netaddr2sockaddr(resp->backups[i].addr);
	    warn << "Backup: " << resp->backups[i].mid << " at "
		 << inet_ntoa(sin.sin_addr) << ":" << resp->backups[i].addr.port << "\n";
	}
	exit(0);
    }

    ptr<aclnt> c_, bc_;
};
/* cs244b: end cut for lab */

int
main(int ac, char **av)
{
    /* cs244b: start cut for lab */
    if (ac != 2)
	fatal << "Usage: " << av[0] << " group-udp-port\n";

    char *group_port = av[1];
    vNew refcounted<groupstat>(atoi(group_port));

    amain();
    /* cs244b: end cut for lab */
}
