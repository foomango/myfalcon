/* 
 * Copyright (c) 2011 Joshua B. Leners (University of Texas at Austin).
 * All rights reserved.
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of Texas at Austin. The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 */

#include "dsm.h"

static uint32_t dsm_counter_ = 0;

bool_t
dsm_null_1_svc(void *argp, void *result, struct svc_req *rqstp) {
    return 1;
}

bool_t
dsm_inc_1_svc(void *argp, dsm_count *result, struct svc_req *rqstp) {
    dsm_counter_++;
    *result = dsm_counter_;
    return 1;
}

bool_t
dsm_count_1_svc(void *argp, dsm_count *result, struct svc_req *rqstp) {
    *result = dsm_counter_;
    return 1;
}

int
dsm_prog_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
            return 1;
}
