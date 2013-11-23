# Copyright (c) 2011 Joshua B. Leners (University of Texas at Austin).
# All rights reserved.
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of Texas at Austin. The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 

from socket import socket, AF_INET, SOCK_DGRAM
from subprocess import Popen
from os import fork, setsid, umask, dup2
from sys import stdin, stdout, stderr


crash_file = "/sys/kernel/debug/provoke-crash/DIRECT"
log = None

def DoFailure(failure):
    failure = failure.strip()
    if failure == "qemu":
        Popen(["pkill", "-TERM", failure], stdout=log, stderr=log).wait()
    elif failure in ["libvirtd", "qemu", "os_enforcer", "os_worker", "incrementer", "process_enforcer", "vmm_observer"]:
        Popen(["pkill", "-KILL", failure], stdout=log, stderr=log).wait()
    elif failure == "hypervisor":
        Popen(["ifconfig", "br0", "down"], stdout=log, stderr=log).wait()
    else:
        with open(crash_file, "w") as outf:
            if failure == "fail_loop":
                outf.write("LOOP\n")
            elif failure == "fail_overflow":
                outf.write("OVERFLOW\n")
            elif failure == "fail_panic":
                outf.write("PANIC\n")
            elif failure == "fail_handlerd":
                Popen(["pkill", "-KILL", "handlerd"], stdout=log, stderr=log).wait()


if __name__ == "__main__":
    if fork() != 0: 
        exit(0)
    umask(0) 
    setsid() 
    if fork() != 0:
        exit(0)
    stdout.flush()
    stderr.flush()
    si = file('/dev/null', 'r')
    so = file('/dev/null', 'a+')
    se = file('/dev/null', 'a+', 0)
    dup2(si.fileno(), stdin.fileno())
    dup2(so.fileno(), stdout.fileno())
    dup2(se.fileno(), stderr.fileno())
    import time, os, sys
    s = socket(AF_INET, SOCK_DGRAM)
    s.bind(("0.0.0.0", 12345))

    log = open("/tmp/fault_server.log", "w+")
    sys.stdout = log
    sys.stderr = log
    while True:
        failure = s.recv(256)
        log.write("%f:%s\n" % (time.time(), failure))
        log.flush()
        os.fsync(log.fileno())
        DoFailure(failure)
