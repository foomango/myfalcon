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

int32_t counter = 0;

uint32_t
MySpy() {
    counter++;
    return (3 != counter) ? PROC_OBS_ALIVE : PROC_OBS_DEAD;
}

const char *my_handle = "ntfa_checkstate";

int
main () {
    SetHandle(my_handle);
    SetSpy(&MySpy, my_handle);
    for(;;);
    return 0;
}
