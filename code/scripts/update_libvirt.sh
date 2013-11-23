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
if [ $USER != "ntfa" ] 
then
    echo Should only run this script as ntfa
    exit 1
fi
cd /home/ntfa/libvirt && git checkout master && git reset --hard && git pull --rebase && make && sudo make install && sudo /etc/init.d/libvirtd restart
