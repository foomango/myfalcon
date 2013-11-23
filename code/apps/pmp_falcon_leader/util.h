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

#ifndef REP_UTIL_H
#define REP_UTIL_H 1

#include "set.h"
using namespace sfs;

/*
 * Various useful operators for our data types.
 */

inline bool
operator==(const viewid_t &a, const viewid_t &b)
{
    return a.counter == b.counter && a.manager == b.manager;
}

inline bool
operator!=(const viewid_t &a, const viewid_t &b)
{
    return a.counter != b.counter || a.manager != b.manager;
}

inline bool
operator<(const viewid_t &a, const viewid_t &b)
{
    return a.counter < b.counter || (a.counter == b.counter && a.manager < b.manager);
}

inline viewstamp_t
operator+(const viewstamp_t &a, const uint32_t &i)
{
    viewstamp_t n = a;
    n.ts += i;
    if (n.ts < a.ts)
	fatal << "timestamp wraparound, time for a view change\n";
    return n;
}

inline bool
operator<(const viewstamp_t &a, const viewstamp_t &b)
{
    return a.vid < b.vid || (a.vid == b.vid && a.ts < b.ts);
}

inline bool
operator==(const viewstamp_t &a, const viewstamp_t &b)
{
    return a.vid == b.vid && a.ts == b.ts;
}

inline bool
operator<=(const viewstamp_t &a, const viewstamp_t &b)
{
    return a < b || a == b;
}

inline bool
operator!=(const viewstamp_t &a, const viewstamp_t &b)
{
    return a.vid != b.vid || a.ts != b.ts;
}

inline viewstamp_t
operator++(viewstamp_t &a)
{
    a = a + 1;
    return a;
}

inline bool
operator==(const cohort_t &a, const cohort_t &b)
{
    return a.mid == b.mid;
}

inline bool
operator<(const cohort_t &a, const cohort_t &b)
{
    return a.mid < b.mid;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const viewid_t &vid)
{
    sb << vid.counter << ":" << vid.manager;
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const viewstamp_t &vs)
{
    sb << vs.vid << "." << vs.ts;
    return sb;
}

inline in_addr_t
myipaddr(void)
{
    vec<in_addr> addrs;
    if (!myipaddrs(&addrs))
	fatal << "Cannot obtain local IP address\n";

    for (int i = 0; i < addrs.size(); i++) {
	in_addr_t a = ntohl(addrs[i].s_addr);
	if (a != INADDR_LOOPBACK)
	    return a;
    }

    fatal << "Cannot obtain local IP address -- nothing usable\n";
}

inline uint64_t
unique_process_id(void)
{
    uint64_t id = myipaddr();
    id <<= 32;
    id |= getpid();
    return id;
}

inline sockaddr_in
netaddr2sockaddr(net_address_t na)
{
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(na.ipaddr);
    sin.sin_port = htons(na.port);
    return sin;
}

class rpccb_unreliable_sockaddrin : public rpccb_unreliable {
 public:
    rpccb_unreliable_sockaddrin(ref<aclnt> a, xdrsuio &x, aclnt_cb cb,
				void *out, xdrproc_t outproc,
				const sockaddr *sa)
	: sin(*(sockaddr_in *) sa),
	  rpccb_unreliable(a, x, cb, out, outproc, (const sockaddr *) &sin) {}

 private:
    sockaddr_in sin;
};

inline ptr<set<mid_t> >
view2mids(const view_t &v) {
    ptr<set<mid_t> > s = New refcounted<set<mid_t> >();
    s->insert(v.primary.mid);
    for (int i = 0; i < v.backups.size(); i++)
	s->insert(v.backups[i].mid);
    return s;
}

inline ptr<set<cohort_t> >
view2cohorts(const view_t &v) {
    ptr<set<cohort_t> > s = New refcounted<set<cohort_t> >();
    s->insert(v.primary);
    for (int i = 0; i < v.backups.size(); i++)
	s->insert(v.backups[i]);
    return s;
}

#endif
