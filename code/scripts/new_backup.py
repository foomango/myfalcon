#! /usr/bin/python
#
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

from boto.s3.connection import S3Connection
from boto.s3.key import Key
from subprocess import check_output, Popen, PIPE
from tempfile import TemporaryFile
import sys

def get_aws_directory_set(my_prefix, bucket):
  plist = []
  for item in bucket.list(prefix=my_prefix, delimiter="/"):
    plist.append(item.name.split("/")[-2])
  return set(plist)

def get_data_node_directory_list(data_node, master_dir):
  return check_output(["ssh", data_node, "ls", master_dir]).split()

def backup_node(prefix, data_node, aws_list):
  pass

def get_backup_list(aws_list, data_node_list):
  ret = []
  for item in data_node_list:
    if item not in aws_list:
      ret.append(item)
  return ret
  
def backup_dir(key, data_node, directory):
  temp = TemporaryFile()
  archiver = Popen(["ssh", data_node, "tar", "c", directory], stdout=PIPE)
  compressor = Popen(["lzma", "-z", "-9"], stdin=archiver.stdout, stdout=temp)
  compressor.wait()
  temp.seek(0)
  key.set_contents_from_file(temp)

def backup(bucket, nodelist, node_prefix, aws_prefix):
  aws_set = get_aws_directory_set(aws_prefix, bucket)
  for host in nodelist:
    local_list = get_data_node_directory_list(host, node_prefix)
    if local_list == []: continue
    backup_list = get_backup_list(aws_set, local_list)

    for item in backup_list:
        print "backing up", item
        key = Key(bucket, aws_prefix + item + "/object.lzma")
        backup_dir(key, host, node_prefix + item)

def backup_run_data(data_dir, node):
  conn = S3Connection()
  bucket = conn.get_bucket("ntfa")
  node_prefix = "/home/ntfa/ntfa_data/"
  aws_prefix = "ntfa_data/ntfa_data/"
  print "backing up", data_dir
  key = Key(bucket, aws_prefix + data_dir + "/object.lzma")
  backup_dir(key, node, node_prefix + data_dir)

if __name__ == "__main__":
  conn = S3Connection()
  bucket = conn.get_bucket("ntfa")
  
  assert(len(sys.argv) > 2)

  node = sys.argv[1]
  data_dir = sys.argv[2]
  
  backup_run_data(data_dir, node)
