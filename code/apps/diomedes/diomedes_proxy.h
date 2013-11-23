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

#ifndef _NTFA_DIOMEDES_DIOMEDES_PROXY_H_
#define _NTFA_DIOMEDES_DIOMEDES_PROXY_H_

#include <stdint.h>

struct client_info {
    uint32_t addr;
    uint16_t port;

    client_info(const uint32_t a, const uint16_t p) :
        addr(a), port(p) {}
    
    client_info(const client_info& c) {
        addr = c.addr;
        port = c.port;
    }

    client_info() :
        addr(0), port(0) {}

    bool operator<(const client_info& c) const 
        { return (addr < c.addr) && (port < c.port); }
};

#endif // _NTFA_DIOMEDES_DIOMEDES_PROXY_H_
