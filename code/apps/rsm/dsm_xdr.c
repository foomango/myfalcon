/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "dsm.h"

bool_t
xdr_dsm_count (XDR *xdrs, dsm_count *objp)
{
	register int32_t *buf;

	 if (!xdr_uint32_t (xdrs, objp))
		 return FALSE;
	return TRUE;
}
