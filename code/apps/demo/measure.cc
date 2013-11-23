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

#include "common/common.h"
#include "pfd/pfd.h"
#include <unistd.h>
#include <stdio.h>

const int STAT_SIZE = (150000) * 10;

double array[STAT_SIZE];

int
main (int argc, char **argv) {
    PFD *pfd = PFD::Instance();
    Application *app_id;
    if (argc == 3) {
        app_id = new Application(argv[1], argv[2]);
    } else if (argc == 4) {
        app_id = new Application(argv[1], argv[2], argv[3]);
        LOG("Querying application %s:%s on vmm %s", argv[1], argv[2], argv[3]);
    }
    ApplicationInstance *app_instance = pfd->GetAppInstance(app_id);
    for (;;) {
        pfd->GetAppStatus(app_instance);
    }
    delete app_instance;
    return 0;
}
