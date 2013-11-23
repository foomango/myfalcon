// -*-c++-*-
/* This file was automatically generated by rpcc. */

#ifndef __RPCC_CLIENT_PROT_H_INCLUDED__
#define __RPCC_CLIENT_PROT_H_INCLUDED__ 1

#include "xdrmisc.h"


struct client_up_arg {
  rpc_str<RPC_INFINITY> handle;
  rpc_vec<uint32_t, RPC_INFINITY> generation;
  uint32_t client_tag;
};
void *client_up_arg_alloc ();
bool_t xdr_client_up_arg (XDR *, void *);
RPC_STRUCT_DECL (client_up_arg)

template<class T> bool
rpc_traverse (T &t, client_up_arg &obj)
{
  return rpc_traverse (t, obj.handle)
    && rpc_traverse (t, obj.generation)
    && rpc_traverse (t, obj.client_tag);
}



struct client_down_arg {
  rpc_str<RPC_INFINITY> handle;
  rpc_vec<uint32_t, RPC_INFINITY> generation;
  uint32_t layer_status;
  bool killed;
  bool would_kill;
  uint32_t client_tag;
};
void *client_down_arg_alloc ();
bool_t xdr_client_down_arg (XDR *, void *);
RPC_STRUCT_DECL (client_down_arg)

template<class T> bool
rpc_traverse (T &t, client_down_arg &obj)
{
  return rpc_traverse (t, obj.handle)
    && rpc_traverse (t, obj.generation)
    && rpc_traverse (t, obj.layer_status)
    && rpc_traverse (t, obj.killed)
    && rpc_traverse (t, obj.would_kill)
    && rpc_traverse (t, obj.client_tag);
}


#ifndef CLIENT_PROG
#define CLIENT_PROG 2000112
#endif /* !CLIENT_PROG */
extern const rpc_program client_prog_1;
enum { CLIENT_V1 = 1 };
enum {
  CLIENT_NULL = 0,
  CLIENT_UP = 1,
  CLIENT_DOWN = 2,
};
#define CLIENT_PROG_1_APPLY_NOVOID(macro, void) \
  macro (CLIENT_NULL, void, void) \
  macro (CLIENT_UP, client_up_arg, void) \
  macro (CLIENT_DOWN, client_down_arg, void)
#define CLIENT_PROG_1_APPLY(macro) \
  CLIENT_PROG_1_APPLY_NOVOID(macro, void)

#endif /* !__RPCC_CLIENT_PROT_H_INCLUDED__ */