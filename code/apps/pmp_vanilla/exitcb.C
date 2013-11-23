/* 
 * Copyright (C) 2011 David Mazieres (dm@uun.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#include <async.h>
#include "exitcb.h"

vec<cbv> exitcbs;

static void
exitcb_cleanup(void)
{
    while (exitcbs.size()) {
	cbv cb = exitcbs.pop_back();
	(*cb)();
    }
}

void
exitcb(cbv cb)
{
    static bool inited;
    if (!inited) {
	inited = true;

	sigcb(SIGINT, wrap(exitcb_cleanup));
	sigcb(SIGQUIT, wrap(exitcb_cleanup));
	sigcb(SIGTERM, wrap(exitcb_cleanup));
	sigcb(SIGHUP, wrap(exitcb_cleanup));
	sigcb(SIGBUS, wrap(exitcb_cleanup));
	sigcb(SIGSEGV, wrap(exitcb_cleanup));
	atexit(exitcb_cleanup);
	fatalhook = &exitcb_cleanup;
    }

    exitcbs.push_back(cb);
}
