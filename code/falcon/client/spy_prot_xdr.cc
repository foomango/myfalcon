/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "spy_prot.h"

bool_t
xdr_spy_status (XDR *xdrs, spy_status *objp)
{

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_status_t (XDR *xdrs, status_t *objp)
{

	 if (!xdr_uint32_t (xdrs, objp))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_gen_no (XDR *xdrs, gen_no *objp)
{

	 if (!xdr_uint32_t (xdrs, objp))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_target_t (XDR *xdrs, target_t *objp)
{

	 if (!xdr_string (xdrs, &objp->handle, ~0))
		 return FALSE;
	 if (!xdr_array (xdrs, (char **)&objp->generation.generation_val, (u_int *) &objp->generation.generation_len, ~0,
		sizeof (gen_no), (xdrproc_t) xdr_gen_no))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_client_addr_t (XDR *xdrs, client_addr_t *objp)
{

	 if (!xdr_uint32_t (xdrs, &objp->ipaddr))
		 return FALSE;
	 if (!xdr_uint32_t (xdrs, &objp->port))
		 return FALSE;
	 if (!xdr_uint32_t (xdrs, &objp->client_tag))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_spy_register_arg (XDR *xdrs, spy_register_arg *objp)
{

	 if (!xdr_target_t (xdrs, &objp->target))
		 return FALSE;
	 if (!xdr_bool (xdrs, &objp->lethal))
		 return FALSE;
	 if (!xdr_int32_t (xdrs, &objp->up_interval_ms))
		 return FALSE;
	 if (!xdr_client_addr_t (xdrs, &objp->client))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_spy_cancel_arg (XDR *xdrs, spy_cancel_arg *objp)
{

	 if (!xdr_target_t (xdrs, &objp->target))
		 return FALSE;
	 if (!xdr_client_addr_t (xdrs, &objp->client))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_spy_kill_arg (XDR *xdrs, spy_kill_arg *objp)
{

	 if (!xdr_target_t (xdrs, &objp->target))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_spy_get_gen_arg (XDR *xdrs, spy_get_gen_arg *objp)
{

	 if (!xdr_target_t (xdrs, &objp->target))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_spy_res (XDR *xdrs, spy_res *objp)
{

	 if (!xdr_target_t (xdrs, &objp->target))
		 return FALSE;
	 if (!xdr_status_t (xdrs, &objp->status))
		 return FALSE;
	return TRUE;
}
