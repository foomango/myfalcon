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
#ifndef _NTFA_COMMON_MESSAGES_H_
#define _NTFA_COMMON_MESSAGES_H_
#include "common.h"

#define IP_BUF_SIZE 32
#define HOSTNAME_SIZE 128
// Handlerd related messages
/*
struct handlerd_unix_handshake {
    handle_t handle;
    uint32_t pid;
} __attribute__((__packed__));

struct handlerd_unix_reply {
    app_state_t state;
} __attribute__((__packed__));

struct handlerd_query_response {
    handle_t handle;
    uint32_t last_generation;
    char     state;
} __attribute__((__packed__));

struct handlerd_unix_probe {
    uint64_t garbage;
} __attribute__((__packed__));

struct handlerd_query {
    handle_t    dst_handle;
    uint32_t    spy_timeout_ms;
    uint32_t    confirm_wait_ms;
    uint32_t    app_generation;
    uint32_t    domain_generation;
    uint32_t    hypervisor_generation;
} __attribute__((__packed__));

// Router Spy related messages
struct router_query {
    char    hostname[HOSTNAME_SIZE];
    uint32_t generation_num;
    unsigned timeout_ms;
} __attribute__((__packed__));

struct router_response {
    char        hostname[HOSTNAME_SIZE];
    uint32_t    last_generation;
    char        state;
} __attribute__((__packed__));

struct router_handshake {
    uint32_t generation_num;
} __attribute__((__packed__));

struct libvirtd_handshake {
    char hostname[HOSTNAME_SIZE];
} __attribute__((__packed__));

struct keepalive_msg {
    uint32_t counter;
} __attribute__((__packed__));
*/
struct pfd_server_handshake {
    char app_handle[HOSTNAME_SIZE];  // eg. zookeeper
    char host_name[HOSTNAME_SIZE];   // eg. wlhung1, wlhung2, ...
    char vmm_name[HOSTNAME_SIZE];    // eg. tri, dva, ...
    char switch_name[HOSTNAME_SIZE]; // eg. router
    //long handlerd_delay_ms;
    //long vmspy_delay_ms;
    //long switch_delay_ms;
    //long handlerd_spy_timeout_ms;
    //long handlerd_confirm_timeout_ms;
    //long vmspy_timeout_ms;
    //long switch_timeout_ms;
} __attribute__((__packed__));

struct pfd_server_reply {
    uint64_t state;
} __attribute__((__packed__));

struct pfd_server_prob {
    uint64_t garbage;
} __attribute__((__packed__));


/* Legacy
struct vmspy_query {
    char        vm_name[32];
    uint32_t    delay_ms;
} __attribute__((__packed__));

struct vmspy_response {
    char    vm_name[32];
    char    status;
} __attribute__((__packed__));
*/
#endif // _NTFA_COMMON_MESSAGES_H_
