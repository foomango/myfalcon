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
START_DIR=`pwd`
TMPDIR=/tmp/falcon_sloccount
echo "This script may take a while" 1>&2
mkdir -p $TMPDIR
cd $TMPDIR
git clone -q ssh://skorokhod.csres.utexas.edu/usr/local/repos/libvirt
git clone -q ssh://skorokhod.csres.utexas.edu/usr/local/repos/zookeeper
cd libvirt
sloccount . | gawk -F'=' '/\(SLOC\)/{if ($2 ~/[0=9]/) {print "modified_libvirt", $2}}'
cd daemon
sloccount . | gawk -F'=' '/\(SLOC\)/{if ($2 ~/[0=9]/) {print "modified_libvirtd", $2}}'
cd ..
# this is the commit we pulled off of the mainline
git checkout -q e37ff2004a891f31016ee747030b88050a83b479
sloccount . | gawk -F'=' '/\(SLOC\)/{if ($2 ~/[0=9]/) {print "vanilla_libvirt", $2}}'
cd daemon
sloccount . | gawk -F'=' '/\(SLOC\)/{if ($2 ~/[0=9]/) {print "vanilla_libvirtd", $2}}'
cd $START_DIR
rm -fr $TMPDIR
