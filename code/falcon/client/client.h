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
#ifndef _NTFA_FALCON_CLIENT_CLIENT_H_
#define _NTFA_FALCON_CLIENT_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "client_status.h"

struct falcon_target;
namespace Falcon {
// For convenience
typedef std::string LayerId;
typedef std::vector<LayerId> LayerIdList;

// This is the raw function passed by the client
// handle        is the fully qualified handle of the application for this
//               callback
// client_data   pointer to opaque data for use by the callback
// falcon_status indicates whether this was caused by a remote or local event
//               if it is a remote event, it indicates the layer
// remote_status this is the status provided by the remote spy
typedef void(*falcon_callback_fn)(const LayerIdList& handle,
                                  void* client_data,
                                  uint32_t falcon_status,
                                  uint32_t remote_status);


void LogCallbackData(const LayerIdList& id_list, uint32_t falcon_status,
                     uint32_t remote_st);

falcon_target* init(const LayerIdList& id_list, bool lethal, void* client_data,
                    int up_callback_period=-1);

void uninit(falcon_target* target);

void startMonitoring(falcon_target* target, falcon_callback_fn cb, int e2etimeout=300);
void stopMonitoring(falcon_target* target);

void startTimer(falcon_target* target, int e2etimeout);
void stopTimer(falcon_target* target);

void setCallback(falcon_target* target, falcon_callback_fn cb);
void removeCallback(falcon_target* target);

bool query_alive(falcon_target* target);
};
#endif  // _NTFA_FALCON_CLIENT_CLIENT_H_
