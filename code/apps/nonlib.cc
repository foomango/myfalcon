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


            res.primary_c = view_.primary;
    char hostname[32];
    gethostname(hostname, 32);
    char *hypervisor = av[4];
// Falcon state
std::map<mid_t,int> mid2fdfd_;
std::list<mid_t> active_servers_;
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
 
