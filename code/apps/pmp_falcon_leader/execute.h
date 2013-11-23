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
 * Interface to a state machine implemented as an RPC server.
 */

typedef callback<void, str>::ptr execcb_t;
typedef callback<void, str, execcb_t>::ptr execute_t;

struct exec_request {
    str input;
    execcb_t cb;
};

class exec_server : public virtual refcount {
 public:
    exec_server(str host, uint16_t port) : host_(host), port_(port) {
    }

    void execute(str input, execcb_t cb) {
	exec_request r;
	r.input = input;
	r.cb = cb;
	rq_.push_back(r);
	if (rq_.size() == 1)
	    sendreq();
    }

 private:
    void connect_cb(int fd) {
	make_async(fd);
	x_ = axprt_stream::alloc(fd);
	sendreq();
    }

    void recv(const char *pkt, ssize_t len, const sockaddr *addr) {
	if (!pkt) {
	    x_ = 0;
	    if (rq_.size())
		sendreq();
	    return;
	}

	exec_request r = rq_.pop_front();
	(*r.cb)(str(pkt, len));

	if (rq_.size())
	    sendreq();
    }

    void sendreq() {
	if (!x_) {
	    tcpconnect(host_, port_, wrap(mkref(this), &exec_server::connect_cb));
	    return;
	}

	exec_request r = rq_[0];
	x_->send(r.input.cstr(), r.input.len(), 0);
	x_->setrcb(wrap(mkref(this), &exec_server::recv));
    }

    str host_;
    uint16_t port_;
    ptr<axprt> x_;
    vec<exec_request> rq_;
};
