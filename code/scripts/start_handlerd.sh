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
sudo rm -fr /tmp/falcon
cd ~/ntfa/code/bin
sudo insmod /usr/src/linux/drivers/misc/lkdtm.ko
sudo ./incrementer
sudo ./handlerd $@
sudo python /home/ntfa/ntfa/code/scripts/vmm/fault_server.py
sudo bash -c "echo -17 > /proc/$(pgrep -G0 handlerd)/oom_adj"
