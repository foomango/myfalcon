#!/bin/sh
java org/apache/zookeeper/test/fd/MonitorApplication server.json &
sleep 1
java org/apache/zookeeper/test/fd/DummyApplication client.json &
wait
