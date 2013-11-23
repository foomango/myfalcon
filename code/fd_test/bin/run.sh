#!/bin/sh
java org/apache/zookeeper/test/fd/MonitorApplication /tmp/fdserver.cfg &
sleep 5
java org/apache/zookeeper/test/fd/DummyApplication /tmp/fdclient.cfg &
wait
