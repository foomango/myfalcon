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

#ifndef _NTFA_RSM_PB_DIOMEDES_H_
#define _NTFA_RSM_PB_DIOMEDES_H_
#include <stdint.h>

namespace Diomedes {
// TODO(leners): move to messages? Common? leave here?
const uint16_t kDiomedesClientPort = 1184; // End of the Trojan war
const uint16_t kDiomedesServerPort = 1185; // Year before end of Trojan war
const int32_t kBufferSize = 2048;
const char* kSwitchName = "router";
const uint32_t kDefaultPollIntervalMilliseconds = 50;
const uint32_t kDefaultBackupPollIntervalMilliseconds = 500;
const uint32_t kNumEpollEvents = 10;

}
#endif // _NTFA_RSM_PB_DIOMEDES_H_
