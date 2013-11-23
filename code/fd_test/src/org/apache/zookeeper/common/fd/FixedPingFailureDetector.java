/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.apache.zookeeper.common.fd;

import java.util.HashMap;
import java.util.Map;

/**
 * A simple implementation of a ping failure detector. 
 * This implementation relies on the application to set the 
 * timeouts of the monitored objects, and
 * these timeouts remain static.
 */
public class FixedPingFailureDetector implements FailureDetector {

    private Map<String, Monitored> monitoreds = new HashMap<String, Monitored>();

    public FixedPingFailureDetector() {}
    
    public FixedPingFailureDetector(Map<String, String> parameters) {
        this();
    }
    
    @Override
    public void updatePingSample(String id, long now,
            long interArrivalMean, long interArrivalStdDev) {
        messageReceived(id, now, MessageType.PING);
    }
    
    @Override
    public void registerMonitored(String id, long now, long timeout) {
        Monitored monitored = monitoreds.get(id);
        if (monitored != null) {
            throw new IllegalArgumentException("Object with id " + id
                    + " is already monitored");
        }
        monitored = new Monitored(id);
        monitored.lastHeard = now;
        monitored.lastSent = now;
        monitored.timeout = timeout;
        monitored.pingInterval = timeout / 2;

        monitoreds.put(id, monitored);
    }

    @Override
    public void setTimeout(String id, long timeout) {
        Monitored monitored = getMonitored(id);
        monitored.timeout = timeout;
    }

    @Override
    public boolean isFailed(String id, long now) {
        Monitored monitored = monitoreds.get(id);
        if (monitored == null) {
            return false;
        }
        if (now > monitored.lastHeard + getTimeout(monitored.id)) {
            return true;
        }
        return false;
    }

    @Override
    public boolean shouldPing(String id, long now) {
        if (!monitoreds.containsKey(id)) {
            return false;
        }
        if (getTimeToNextPing(id, now) <= 0) {
            return true;
        }
        return false;
    }


    @Override
    public long getIdleTime(String id, long now) {
        Monitored monitored = getMonitored(id);
        return now - monitored.lastHeard;
    }

    @Override
    public long getTimeout(String id) {
        return getMonitored(id).timeout;
    }

    @Override
    public long getTimeToNextPing(String id, long now) {
        Monitored monitored = getMonitored(id);
        return monitored.pingInterval - (now - monitored.lastSent);
    }

    @Override
    public void releaseMonitored(String id) {
        monitoreds.remove(id);
    }

    private Monitored getMonitored(String id) {
        Monitored monitored = monitoreds.get(id);
        if (monitored == null) {
            throw new IllegalArgumentException(
                    "The monitored must be registered first");
        }
        return monitored;
    }

    private static class Monitored {
        String id;

        long timeout;
        long lastSent;
        long lastHeard;
        long pingInterval;

        public Monitored(String id) {
            this.id = id;
        }
    }

    @Override
    public void messageReceived(String id, long now, MessageType type) {
        Monitored monitored = getMonitored(id);
        monitored.lastHeard = now;
    }

    @Override
    public void messageSent(String id, long now, MessageType type) {
        Monitored monitored = getMonitored(id);
        monitored.lastSent = now;
    }

    @Override
    public void setPingInterval(String id, long interval) {
        Monitored monitored = getMonitored(id);
        monitored.pingInterval = interval;
    }

    /**
     * Hacking function
     */
    public InterArrivalSamplingWindow getSampWindow(String id) {
        return null;
    }

    /**
     * Hacking function
     */
    public void setSampWindow(String id, InterArrivalSamplingWindow samplingWindow) {
    }
}
