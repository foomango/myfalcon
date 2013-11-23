// -*-c++-*-
/* This file was automatically generated by rpcc. */

#ifndef __RPCC_REP_H_INCLUDED__
#define __RPCC_REP_H_INCLUDED__ 1

#include "xdrmisc.h"

typedef u_int64_t mid_t;
void *mid_t_alloc ();
bool_t xdr_mid_t (XDR *, void *);
RPC_TYPEDEF_DECL (mid_t)

typedef u_int64_t rid_t;
void *rid_t_alloc ();
bool_t xdr_rid_t (XDR *, void *);
RPC_TYPEDEF_DECL (rid_t)


struct viewid_t {
  u_int64_t counter;
  mid_t manager;
};
void *viewid_t_alloc ();
bool_t xdr_viewid_t (XDR *, void *);
RPC_STRUCT_DECL (viewid_t)

template<class T> bool
rpc_traverse (T &t, viewid_t &obj)
{
  return rpc_traverse (t, obj.counter)
    && rpc_traverse (t, obj.manager);
}



struct net_address_t {
  u_int32_t ipaddr;
  u_int32_t port;
};
void *net_address_t_alloc ();
bool_t xdr_net_address_t (XDR *, void *);
RPC_STRUCT_DECL (net_address_t)

template<class T> bool
rpc_traverse (T &t, net_address_t &obj)
{
  return rpc_traverse (t, obj.ipaddr)
    && rpc_traverse (t, obj.port);
}



struct cohort_t {
  mid_t mid;
  net_address_t addr;
};
void *cohort_t_alloc ();
bool_t xdr_cohort_t (XDR *, void *);
RPC_STRUCT_DECL (cohort_t)

template<class T> bool
rpc_traverse (T &t, cohort_t &obj)
{
  return rpc_traverse (t, obj.mid)
    && rpc_traverse (t, obj.addr);
}



struct execute_viewinfo {
  viewid_t vid;
  net_address_t primary;
};
void *execute_viewinfo_alloc ();
bool_t xdr_execute_viewinfo (XDR *, void *);
RPC_STRUCT_DECL (execute_viewinfo)

template<class T> bool
rpc_traverse (T &t, execute_viewinfo &obj)
{
  return rpc_traverse (t, obj.vid)
    && rpc_traverse (t, obj.primary);
}



struct execute_arg {
  mid_t client;
  rid_t rid;
  viewid_t vid;
  rpc_bytes<RPC_INFINITY> request;
};
void *execute_arg_alloc ();
bool_t xdr_execute_arg (XDR *, void *);
RPC_STRUCT_DECL (execute_arg)

template<class T> bool
rpc_traverse (T &t, execute_arg &obj)
{
  return rpc_traverse (t, obj.client)
    && rpc_traverse (t, obj.rid)
    && rpc_traverse (t, obj.vid)
    && rpc_traverse (t, obj.request);
}



struct execute_res {
  const bool ok;
  union {
    union_entry_base _base;
    union_entry<rpc_bytes<RPC_INFINITY> > reply;
    union_entry<execute_viewinfo> viewinfo;
  };

#define rpcunion_tag_execute_res ok
#define rpcunion_switch_execute_res(swarg, action, voidaction, defaction) \
  switch (swarg) { \
  case true: \
    action (opaque, reply); \
    break; \
  case false: \
    action (execute_viewinfo, viewinfo); \
    break; \
  default: \
    defaction; \
    break; \
  }

  execute_res (bool _tag = (bool) 0) : ok (_tag)
    { _base.init (); set_ok (_tag); }
  execute_res (const execute_res &_s)
    : ok (_s.ok)
    { _base.init (_s._base); }
  ~execute_res () { _base.destroy (); }
  execute_res &operator= (const execute_res &_s) {
    const_cast<bool &> (ok) = _s.ok;
    _base.assign (_s._base);
    return *this;
  }

  void set_ok (bool _tag) {
    const_cast<bool &> (ok) = _tag;
    rpcunion_switch_execute_res
      (_tag, RPCUNION_SET, _base.destroy (), _base.destroy ());
  }
};

template<class T> bool
rpc_traverse (T &t, execute_res &obj)
{
  bool tag = obj.ok;
  if (!rpc_traverse (t, tag))
    return false;
  if (tag != obj.ok)
    obj.set_ok (tag);

  rpcunion_switch_execute_res
    (obj.ok, RPCUNION_TRAVERSE, return true, return false);
  /* gcc 4.0.3 makes buggy warnings without the following line */
  return false;
}
inline bool
rpc_traverse (const stompcast_t &s, execute_res &obj)
{
  rpcunion_switch_execute_res
    (obj.ok, RPCUNION_REC_STOMPCAST,
     obj._base.destroy (); return true, obj._base.destroy (); return true;);
  /* gcc 4.0.3 makes buggy warnings without the following line */
  return false;
}
void *execute_res_alloc ();
bool_t xdr_execute_res (XDR *, void *);
RPC_UNION_DECL (execute_res)



struct view_t {
  viewid_t vid;
  cohort_t primary;
  rpc_vec<cohort_t, RPC_INFINITY> backups;
};
void *view_t_alloc ();
bool_t xdr_view_t (XDR *, void *);
RPC_STRUCT_DECL (view_t)

template<class T> bool
rpc_traverse (T &t, view_t &obj)
{
  return rpc_traverse (t, obj.vid)
    && rpc_traverse (t, obj.primary)
    && rpc_traverse (t, obj.backups);
}



struct viewstamp_t {
  viewid_t vid;
  u_int32_t ts;
};
void *viewstamp_t_alloc ();
bool_t xdr_viewstamp_t (XDR *, void *);
RPC_STRUCT_DECL (viewstamp_t)

template<class T> bool
rpc_traverse (T &t, viewstamp_t &obj)
{
  return rpc_traverse (t, obj.vid)
    && rpc_traverse (t, obj.ts);
}



struct replicate_arg {
  viewstamp_t vs;
  execute_arg arg;
  viewstamp_t committed;
};
void *replicate_arg_alloc ();
bool_t xdr_replicate_arg (XDR *, void *);
RPC_STRUCT_DECL (replicate_arg)

template<class T> bool
rpc_traverse (T &t, replicate_arg &obj)
{
  return rpc_traverse (t, obj.vs)
    && rpc_traverse (t, obj.arg)
    && rpc_traverse (t, obj.committed);
}



struct replicate_res {
  viewstamp_t vs;
};
void *replicate_res_alloc ();
bool_t xdr_replicate_res (XDR *, void *);
RPC_STRUCT_DECL (replicate_res)

template<class T> inline bool
rpc_traverse (T &t, replicate_res &obj)
{
  return rpc_traverse (t, obj.vs);
}



struct view_change_arg {
  view_t oldview;
  viewid_t newvid;
};
void *view_change_arg_alloc ();
bool_t xdr_view_change_arg (XDR *, void *);
RPC_STRUCT_DECL (view_change_arg)

template<class T> bool
rpc_traverse (T &t, view_change_arg &obj)
{
  return rpc_traverse (t, obj.oldview)
    && rpc_traverse (t, obj.newvid);
}



struct view_change_reject {
  rpc_ptr<view_t> oldview;
  viewid_t newvid;
};
void *view_change_reject_alloc ();
bool_t xdr_view_change_reject (XDR *, void *);
RPC_STRUCT_DECL (view_change_reject)

template<class T> bool
rpc_traverse (T &t, view_change_reject &obj)
{
  return rpc_traverse (t, obj.oldview)
    && rpc_traverse (t, obj.newvid);
}



struct view_change_accept {
  cohort_t myid;
  bool include_me;
  viewstamp_t latest;
  rpc_ptr<view_t> newview;
};
void *view_change_accept_alloc ();
bool_t xdr_view_change_accept (XDR *, void *);
RPC_STRUCT_DECL (view_change_accept)

template<class T> bool
rpc_traverse (T &t, view_change_accept &obj)
{
  return rpc_traverse (t, obj.myid)
    && rpc_traverse (t, obj.include_me)
    && rpc_traverse (t, obj.latest)
    && rpc_traverse (t, obj.newview);
}



struct view_change_res {
  const bool accepted;
  union {
    union_entry_base _base;
    union_entry<view_change_reject> reject;
    union_entry<view_change_accept> accept;
  };

#define rpcunion_tag_view_change_res accepted
#define rpcunion_switch_view_change_res(swarg, action, voidaction, defaction) \
  switch (swarg) { \
  case false: \
    action (view_change_reject, reject); \
    break; \
  case true: \
    action (view_change_accept, accept); \
    break; \
  default: \
    defaction; \
    break; \
  }

  view_change_res (bool _tag = (bool) 0) : accepted (_tag)
    { _base.init (); set_accepted (_tag); }
  view_change_res (const view_change_res &_s)
    : accepted (_s.accepted)
    { _base.init (_s._base); }
  ~view_change_res () { _base.destroy (); }
  view_change_res &operator= (const view_change_res &_s) {
    const_cast<bool &> (accepted) = _s.accepted;
    _base.assign (_s._base);
    return *this;
  }

  void set_accepted (bool _tag) {
    const_cast<bool &> (accepted) = _tag;
    rpcunion_switch_view_change_res
      (_tag, RPCUNION_SET, _base.destroy (), _base.destroy ());
  }
};

template<class T> bool
rpc_traverse (T &t, view_change_res &obj)
{
  bool tag = obj.accepted;
  if (!rpc_traverse (t, tag))
    return false;
  if (tag != obj.accepted)
    obj.set_accepted (tag);

  rpcunion_switch_view_change_res
    (obj.accepted, RPCUNION_TRAVERSE, return true, return false);
  /* gcc 4.0.3 makes buggy warnings without the following line */
  return false;
}
inline bool
rpc_traverse (const stompcast_t &s, view_change_res &obj)
{
  rpcunion_switch_view_change_res
    (obj.accepted, RPCUNION_REC_STOMPCAST,
     obj._base.destroy (); return true, obj._base.destroy (); return true;);
  /* gcc 4.0.3 makes buggy warnings without the following line */
  return false;
}
void *view_change_res_alloc ();
bool_t xdr_view_change_res (XDR *, void *);
RPC_UNION_DECL (view_change_res)



struct new_view_arg {
  viewstamp_t latest;
  view_t view;
};
void *new_view_arg_alloc ();
bool_t xdr_new_view_arg (XDR *, void *);
RPC_STRUCT_DECL (new_view_arg)

template<class T> bool
rpc_traverse (T &t, new_view_arg &obj)
{
  return rpc_traverse (t, obj.latest)
    && rpc_traverse (t, obj.view);
}



struct new_view_res {
  bool accepted;
};
void *new_view_res_alloc ();
bool_t xdr_new_view_res (XDR *, void *);
RPC_STRUCT_DECL (new_view_res)

template<class T> inline bool
rpc_traverse (T &t, new_view_res &obj)
{
  return rpc_traverse (t, obj.accepted);
}



struct view_form_t {
  viewstamp_t prev;
  view_t view;
};
void *view_form_t_alloc ();
bool_t xdr_view_form_t (XDR *, void *);
RPC_STRUCT_DECL (view_form_t)

template<class T> bool
rpc_traverse (T &t, view_form_t &obj)
{
  return rpc_traverse (t, obj.prev)
    && rpc_traverse (t, obj.view);
}



struct ping_arg {
  mid_t myid;
  viewid_t vid;
};
void *ping_arg_alloc ();
bool_t xdr_ping_arg (XDR *, void *);
RPC_STRUCT_DECL (ping_arg)

template<class T> bool
rpc_traverse (T &t, ping_arg &obj)
{
  return rpc_traverse (t, obj.myid)
    && rpc_traverse (t, obj.vid);
}



struct ping_res {
  bool ok;
};
void *ping_res_alloc ();
bool_t xdr_ping_res (XDR *, void *);
RPC_STRUCT_DECL (ping_res)

template<class T> inline bool
rpc_traverse (T &t, ping_res &obj)
{
  return rpc_traverse (t, obj.ok);
}


#ifndef REP_PROG
#define REP_PROG 688884
#endif /* !REP_PROG */
extern const rpc_program rep_prog_1;
enum { REP_V1 = 1 };
enum {
  REP_NULL = 0,
  REP_VIEWINFO = 1,
  REP_EXECUTE = 2,
  REP_REPLICATE = 3,
  REP_FETCHLOG = 4,
  REP_LETMEIN = 5,
  REP_VIEW_CHANGE = 6,
  REP_NEW_VIEW = 7,
  REP_INIT_VIEW = 8,
  REP_PING = 9,
  REP_GET_VIEW = 10,
};
#define REP_PROG_1_APPLY_NOVOID(macro, void) \
  macro (REP_NULL, void, void) \
  macro (REP_VIEWINFO, void, execute_viewinfo) \
  macro (REP_EXECUTE, execute_arg, execute_res) \
  macro (REP_REPLICATE, replicate_arg, replicate_res) \
  macro (REP_FETCHLOG, viewstamp_t, execute_arg) \
  macro (REP_LETMEIN, cohort_t, void) \
  macro (REP_VIEW_CHANGE, view_change_arg, view_change_res) \
  macro (REP_NEW_VIEW, new_view_arg, new_view_res) \
  macro (REP_INIT_VIEW, view_form_t, void) \
  macro (REP_PING, ping_arg, ping_res) \
  macro (REP_GET_VIEW, void, view_t)
#define REP_PROG_1_APPLY(macro) \
  REP_PROG_1_APPLY_NOVOID(macro, void)


#ifndef REP_BCAST_PROG
#define REP_BCAST_PROG 688886
#endif /* !REP_BCAST_PROG */
extern const rpc_program rep_bcast_prog_1;
enum { REP_BCAST_V1 = 1 };
enum {
  REP_BCAST_NULL = 0,
  REP_BCAST_VIEWINFO = 1,
  REP_BCAST_LETMEIN = 2,
};
#define REP_BCAST_PROG_1_APPLY_NOVOID(macro, void) \
  macro (REP_BCAST_NULL, void, void) \
  macro (REP_BCAST_VIEWINFO, void, execute_viewinfo) \
  macro (REP_BCAST_LETMEIN, cohort_t, void)
#define REP_BCAST_PROG_1_APPLY(macro) \
  REP_BCAST_PROG_1_APPLY_NOVOID(macro, void)

#endif /* !__RPCC_REP_H_INCLUDED__ */