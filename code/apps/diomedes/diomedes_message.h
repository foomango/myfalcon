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

#ifndef _NTFA_RSM_PB_DIOMEDES_MESSAGES_H_
#define _NTFA_RSM_PB_DIOMEDES_MESSAGES_H_

#include <stdint.h>
#include <rpc/rpc.h>

namespace Diomedes {

const uint32_t kNameLength = 32;
const uint32_t kMaxDiomedesMessage = 1472; // One ethernet frame
const int32_t kDiomedesHeaderSize = sizeof(char) + sizeof(uint8_t) + sizeof(uint16_t);

typedef enum diomedes_msg_type_e {
    JOIN = 'J',
    VOTE = 'V',
    PRIMARY_REQUEST = 'R',
    ACK = 'A',
    NACK = 'N',
    PRIMARY_JOIN = 'j',
    CLIENT_REQUEST = 'Q',
    CLIENT_RESPONSE = 'S'
} diomedes_message_type;

typedef struct tcp_rpc_msg_s {
    uint32_t    size;
    rpc_msg     msg;
} tcp_rpc_msg;

typedef struct diomedes_id_s {
    char handle[kNameLength];
    char domain[kNameLength];
    char hypervisor[kNameLength];
} __attribute__((__packed__)) diomedes_id;

typedef struct diomedes_join_s {
    diomedes_id joiner;
    diomedes_id primary;
    uint8_t     broadcast; // Only send reply joins to broadcast packets
} __attribute__((__packed__)) diomedes_join;

typedef struct diomedes_primary_req_s {
    uint32_t        req_id;
    // high order bits are a proxy id, low order client id
    uint32_t        client_id;
    tcp_rpc_msg     request; 
} __attribute__((__packed__)) diomedes_primary_request;

typedef diomedes_primary_request diomedes_client_request;
// except request == response
typedef diomedes_primary_request diomedes_client_response;

typedef struct diomedes_vote_s {
    diomedes_id                 voter; 
    diomedes_primary_request    last_req; // Two reasons: 
                                          // (1) So new primary can catchup
                                          // (2) So lossy backup can catchup
} __attribute__((__packed__)) diomedes_vote;

// Only for nacks
typedef struct diomedes_ack_s {
    diomedes_id acker;
    uint32_t    req_id;
} __attribute__((__packed__)) diomedes_ack;

typedef union diomedes_payload_u {
    diomedes_join               join;
    diomedes_vote               vote;
    diomedes_ack                ack;
    diomedes_primary_request    preq;
    diomedes_client_request     creq;
    diomedes_client_response    resp;
} diomedes_payload;

typedef struct diomedes_message_s {
    char                type;
    uint8_t             version;
    uint16_t            length; // size of the payload
    diomedes_payload    payload;
} __attribute__((__packed__)) diomedes_message;

}
#endif // _NTFA_RSM_PB_DIOMEDES_MESSAGES_H_
