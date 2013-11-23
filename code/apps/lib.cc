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

