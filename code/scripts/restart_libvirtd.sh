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

#! /bin/bash
sudo pkill -KILL libvirtd
sudo pkill -f fault_server
sudo rm -fr /tmp/libvirtd.log
sudo rm -rf /usr/local/var/run/libvirtd.pid
sudo /etc/init.d/libvirtd restart
sudo killall os_enforcer
sudo killall os_worker
sudo killall vmm_observer
sudo rm -fr /dev/shm/ntfa.log
sudo /home/ntfa/ntfa/code/falcon/os_enforcer/vmm_observer $@
sudo /home/ntfa/ntfa/code/falcon/os_enforcer/os_enforcer $@
sleep 2
sudo python /home/ntfa/ntfa/code/scripts/vmm/fault_server.py
sudo bash -c "echo -17 > /proc/$(pgrep -G0 os_enforcer)/oom_adj"
