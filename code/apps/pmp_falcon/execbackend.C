/* 
 * Copyright (C) 2011 David Mazieres (dm@uun.org)
 *               2011 Joshua B. Leners (leners@cs.utexas.edu)
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
#include "execbackend.h"

static void
backend_read(int fd, cbi cb)
{
    char buf[256];
    int cc = read(fd, &buf[0], sizeof(buf));
    if (cc <= 0)
	fatal << "error reading from backend stdout: " << strerror(errno) << "\n";
    buf[cc-1] = '\0';

    char *prefix = "Listening on port ";
    if (strncmp(&buf[0], prefix, strlen(prefix)))
	fatal << "funny-looking backend output: " << buf << "\n";

    int port = atoi(&buf[strlen(prefix)]);
    warn << "Got backend server running on port " << port << "\n";

    close(fd);
    fdcb(fd, selread, 0);
    (cb)(port);
}

static void
proc_cleanup(pid_t pid)
{
    kill(SIGKILL, pid);
    exit(-1);
}

void
launch_backend(str pn, cbi cb)
{
    int fds[2];
    assert(pipe(fds) == 0);

    int devnull = open("/dev/null", O_RDONLY);
    assert(devnull >= 0);

    const char *av[] = { pn.cstr(), "0", 0 };
    pid_t pid = spawn(pn, av, devnull, fds[1], 2);
    if (pid < 0)
	fatal << "cannot spawn " << pn << ": " << strerror(errno) << "\n";

    close(devnull);
    close(fds[1]);
    make_async(fds[0]);
    fdcb(fds[0], selread, wrap(backend_read, fds[0], cb));
    exitcb(wrap(proc_cleanup, pid));
}
