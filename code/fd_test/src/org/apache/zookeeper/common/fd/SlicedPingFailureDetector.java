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
 * A simple implementation of a ping failure detector
 * based on time slices. Monitored objects are grouped by
 * their expiration time in time slices. When a time slice
 * expire, all the objects of that slice are considered failed.
 * While updating a monitored, this failure detector 
 * always rounds up the time slice to provide a sort of grace period.
 * This implementation relies on the application to set the 
 * timeouts of the monitored objects, and
 * these timeouts remain static.
 */
public class SlicedPingFailureDetector implements FailureDetector {

    private Map<String, Monitored> monitoreds = new HashMap<String, Monitored>();
    private final long sliceSize;

    protected static final long DEFAULT_SLICE_SIZE = 3000;
    
    /**
     * Creates a SlicedPingFailureDetector with specified 
     * time slice size in milliseconds.
     * 
     * @param sliceSize the time slice size
     */
    public SlicedPingFailureDetector(long sliceSize) {
        this.sliceSize = sliceSize;
    }
    
    /**
     * Create a SlicedPingFailureDetector with default 
     * slice size of 3000 milliseconds.
     */
    public SlicedPingFailureDetector() {
        this(DEFAULT_SLICE_SIZE);
    }
    
    /**
     * Create a SlicedPingFailureDetector from a parameters map
     * @param parameters
     */
    public SlicedPingFailureDetector(Map<String, String> parameters) {
        this(FailureDetectorOptParser.parseLong(
                SlicedPingFailureDetector.DEFAULT_SLICE_SIZE, 
                parameters.get("slice")));
    }
    
    @Override
    public void registerMonitored(String id, long now, long timeout) {
        Monitored monitored = monitoreds.get(id);
        if (monitored != null) {
            throw new IllegalArgumentException("Object with id " + id
                    + " is already monitored");
        }
        monitored = new Monitored();
        monitored.lastHeard = now;
        monitored.timeout = timeout;
        monitored.pingInterval = timeout/2;

        monitoreds.put(id, monitored);
        
        messageReceived(id, now, MessageType.PING);
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
        
        if (now > monitored.expirationSlice) {
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
    public void setPingInterval(String id, long interval) {
        Monitored monitored = getMonitored(id);
        monitored.pingInterval = interval;
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
        Long lastPinged = monitored.lastSent;
        
        return monitored.pingInterval - (now - lastPinged);
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

        long timeout;
        long lastSent;
        long lastHeard;
        long expirationSlice;
        long pingInterval;
        
    }
    
    @Override
    public void messageReceived(String id, long now, MessageType type) {
        Monitored monitored = getMonitored(id);
        long expireTime = roundToInterval(now + monitored.timeout);
        monitored.lastHeard = now;
        
        if (monitored.expirationSlice >= expireTime) {
            return;
        }
        
        monitored.expirationSlice = expireTime;
    }

    @Override
    public void messageSent(String id, long now, MessageType type) {
        Monitored monitored = getMonitored(id);
        monitored.lastSent = now;
    }
    
    private long roundToInterval(long time) {
        // We give a one interval grace period
        return (time / sliceSize + 1) * sliceSize;
    }

    @Override
    public void updatePingSample(String id, long lastHbTimestamp,
            long interArrivalMean, long interArrivalStdDev) {
        messageReceived(id, lastHbTimestamp, MessageType.PING);
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
