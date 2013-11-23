/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _SPY_PROT_H_RPCGEN
#define _SPY_PROT_H_RPCGEN

#include <rpc/rpc.h>

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif


enum spy_status {
	FALCON_SUCCESS = 0,
	FALCON_UNKNOWN_TARGET = 1,
	FALCON_BAD_GEN_VEC = 2,
	FALCON_FUTURE_GEN = 3,
	FALCON_LONG_DEAD = 4,
	FALCON_REGISTER_ACK = 5,
	FALCON_CANCEL_ACK = 6,
	FALCON_CANCEL_ERROR = 7,
	FALCON_KILL_ACK = 8,
	FALCON_GEN_RESP = 9,
	FALCON_UP = 10,
	FALCON_DOWN = 11,
	FALCON_UNKNOWN_ERROR = 12,
};
typedef enum spy_status spy_status;

typedef uint32_t status_t;

typedef uint32_t gen_no;

struct target_t {
	char *handle;
	struct {
		u_int generation_len;
		gen_no *generation_val;
	} generation;
};
typedef struct target_t target_t;

struct client_addr_t {
	uint32_t ipaddr;
	uint32_t port;
	uint32_t client_tag;
};
typedef struct client_addr_t client_addr_t;

struct spy_register_arg {
	target_t target;
	bool_t lethal;
	int32_t up_interval_ms;
	client_addr_t client;
};
typedef struct spy_register_arg spy_register_arg;

struct spy_cancel_arg {
	target_t target;
	client_addr_t client;
};
typedef struct spy_cancel_arg spy_cancel_arg;

struct spy_kill_arg {
	target_t target;
};
typedef struct spy_kill_arg spy_kill_arg;

struct spy_get_gen_arg {
	target_t target;
};
typedef struct spy_get_gen_arg spy_get_gen_arg;

struct spy_res {
	target_t target;
	status_t status;
};
typedef struct spy_res spy_res;

#define SPY_PROG 2000111
#define SPY_V1 1

#if defined(__STDC__) || defined(__cplusplus)
#define SPY_NULL 0
extern  enum clnt_stat spy_null_1(void *, void *, CLIENT *);
extern  bool_t spy_null_1_svc(void *, void *, struct svc_req *);
#define SPY_REGISTER 1
extern  enum clnt_stat spy_register_1(spy_register_arg *, spy_res *, CLIENT *);
extern  bool_t spy_register_1_svc(spy_register_arg *, spy_res *, struct svc_req *);
#define SPY_CANCEL 2
extern  enum clnt_stat spy_cancel_1(spy_cancel_arg *, spy_res *, CLIENT *);
extern  bool_t spy_cancel_1_svc(spy_cancel_arg *, spy_res *, struct svc_req *);
#define SPY_KILL 3
extern  enum clnt_stat spy_kill_1(spy_kill_arg *, spy_res *, CLIENT *);
extern  bool_t spy_kill_1_svc(spy_kill_arg *, spy_res *, struct svc_req *);
#define SPY_GET_GEN 4
extern  enum clnt_stat spy_get_gen_1(spy_get_gen_arg *, spy_res *, CLIENT *);
extern  bool_t spy_get_gen_1_svc(spy_get_gen_arg *, spy_res *, struct svc_req *);
extern int spy_prog_1_freeresult (SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define SPY_NULL 0
extern  enum clnt_stat spy_null_1();
extern  bool_t spy_null_1_svc();
#define SPY_REGISTER 1
extern  enum clnt_stat spy_register_1();
extern  bool_t spy_register_1_svc();
#define SPY_CANCEL 2
extern  enum clnt_stat spy_cancel_1();
extern  bool_t spy_cancel_1_svc();
#define SPY_KILL 3
extern  enum clnt_stat spy_kill_1();
extern  bool_t spy_kill_1_svc();
#define SPY_GET_GEN 4
extern  enum clnt_stat spy_get_gen_1();
extern  bool_t spy_get_gen_1_svc();
extern int spy_prog_1_freeresult ();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_spy_status (XDR *, spy_status*);
extern  bool_t xdr_status_t (XDR *, status_t*);
extern  bool_t xdr_gen_no (XDR *, gen_no*);
extern  bool_t xdr_target_t (XDR *, target_t*);
extern  bool_t xdr_client_addr_t (XDR *, client_addr_t*);
extern  bool_t xdr_spy_register_arg (XDR *, spy_register_arg*);
extern  bool_t xdr_spy_cancel_arg (XDR *, spy_cancel_arg*);
extern  bool_t xdr_spy_kill_arg (XDR *, spy_kill_arg*);
extern  bool_t xdr_spy_get_gen_arg (XDR *, spy_get_gen_arg*);
extern  bool_t xdr_spy_res (XDR *, spy_res*);

#else /* K&R C */
extern bool_t xdr_spy_status ();
extern bool_t xdr_status_t ();
extern bool_t xdr_gen_no ();
extern bool_t xdr_target_t ();
extern bool_t xdr_client_addr_t ();
extern bool_t xdr_spy_register_arg ();
extern bool_t xdr_spy_cancel_arg ();
extern bool_t xdr_spy_kill_arg ();
extern bool_t xdr_spy_get_gen_arg ();
extern bool_t xdr_spy_res ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_SPY_PROT_H_RPCGEN */