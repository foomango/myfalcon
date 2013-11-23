/* 
 * Copyright (C) 2011 Joshua B. Leners <leners@cs.utexas.edu> and
 * the University of Texas at Austin
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

#ifndef _NTFA_VMM_ENFORCER_VMM_ENFORCER_H_
#define _NTFA_VMM_ENFORCER_VMM_ENFORCER_H_
#include <stdint.h>
enum vmm_enforcer_stat {
    VMM_TIMEDOUT,
    VMM_CANCELED
};

const uint16_t kDefaultVMMProbePort = 2001;
#endif  // _NTFA_VMM_ENFORCER_VMM_ENFORCER_H_