/*
 * Replication protocol.
 */

typedef unsigned hyper mid_t;	    /* globally unique machine identifier */
typedef unsigned hyper rid_t;	    /* per-client unique request identifier */

struct viewid_t {
    unsigned hyper counter;
    mid_t manager;
};

struct net_address_t {
    unsigned int ipaddr;
    unsigned int port;
};

struct cohort_t {
    mid_t mid;
    net_address_t addr;
};

struct execute_viewinfo {
    viewid_t vid;
    net_address_t primary;
};

struct execute_arg {
    mid_t client;
    rid_t rid;
    viewid_t vid;
    opaque request<>;
};

union execute_res switch (bool ok) {
 case TRUE:
    opaque reply<>;
 case FALSE:
    execute_viewinfo viewinfo;
};

struct view_t {
    viewid_t vid;
    cohort_t primary;
    cohort_t backups<>;
};

struct viewstamp_t {
    viewid_t vid;
    unsigned ts;
};

struct replicate_arg {
    viewstamp_t vs;
    execute_arg arg;
    viewstamp_t committed;
};

struct replicate_res {
    viewstamp_t vs;
};

struct view_change_arg {
    view_t oldview;
    viewid_t newvid;
};

struct view_change_reject {
    view_t *oldview;
    viewid_t newvid;
};

struct view_change_accept {
    cohort_t myid;
    bool include_me;
    viewstamp_t latest;
    view_t *newview;
};

union view_change_res switch (bool accepted) {
 case FALSE:
    view_change_reject reject;
 case TRUE:
    view_change_accept accept;
};

struct new_view_arg {
    viewstamp_t latest;
    view_t view;
};

struct new_view_res {
    bool accepted;
};

/* marshaled into execute_arg.request when ts==0 */
struct view_form_t {
    viewstamp_t prev;
    view_t view;
};

struct ping_arg {
    mid_t myid;
    viewid_t vid;
};

struct ping_res {
    bool ok;
};

program REP_PROG {
    version REP_V1 {
	void
	REP_NULL(void) = 0;

	execute_viewinfo
	REP_VIEWINFO(void) = 1;

	execute_res
	REP_EXECUTE(execute_arg) = 2;

	replicate_res
	REP_REPLICATE(replicate_arg) = 3;

	execute_arg
	REP_FETCHLOG(viewstamp_t) = 4;

	void
	REP_LETMEIN(cohort_t) = 5;

	view_change_res
	REP_VIEW_CHANGE(view_change_arg) = 6;

	new_view_res
	REP_NEW_VIEW(new_view_arg) = 7;

	void
	REP_INIT_VIEW(view_form_t) = 8;

	ping_res
	REP_PING(ping_arg) = 9;

	/* cs244b: start cut for lab */
	view_t
	REP_GET_VIEW(void) = 10;
	/* cs244b: end cut for lab */
    } = 1;
} = 688884;

program REP_BCAST_PROG {
    version REP_BCAST_V1 {
	void
	REP_BCAST_NULL(void) = 0;

	execute_viewinfo
	REP_BCAST_VIEWINFO(void) = 1;

	void
	REP_BCAST_LETMEIN(cohort_t) = 2;
    } = 1;
} = 688886;

