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

#include "common.h"
#include "process_enforcer/process_observer.h"

uint32_t
MySpy(void) {
    for (;;) sleep(10);
    return PROC_OBS_ALIVE;
}

const char *my_handle = "ntfa_spin_stuck";

int
main () {
    SetHandle(my_handle);
    SetSpy(&MySpy, my_handle, DELAY_CPUTIME, 20);
    for (;;) sleep(10);
    return 0;
}
