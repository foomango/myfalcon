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
 * Failure detector implementation according to 
 * Chen method, as described in its paper 'On the 
 * Quality of Service of Failure Detectors'. Chen's method 
 * uses the average of the received pings timestamps added 
 * to a safety margin parameter called alpha in order to 
 * estimated the next timeout.
 */
public class ChenFailureDetector implements FailureDetector {

    protected static final long DEFAULT_ALPHA = 1250;

    /**
     * Sampling window size
     */
    private static final int N = 1000;
    
    
    private Map<String, Monitored> monitoreds = new HashMap<String, Monitored>();
    private long alpha;
    
    /**
     * Create a ChenFailureDetector with specified alpha parameter
     * @param alpha safety margin parameter
     */
    public ChenFailureDetector(long alpha) {
        this.alpha = alpha;
    }
    
    /**
     * Create a ChenFailureDetector from a parameters map
     * @param parameters
     */
    public ChenFailureDetector(Map<String, String> parameters) {
        this(FailureDetectorOptParser.parseLong(
                ChenFailureDetector.DEFAULT_ALPHA, parameters.get("alpha")));
    }
    
    /**
     * Create a ChenFailureDetector with default values 
     * for the alpha parameter: alpha = 1250 ms
     */
    public ChenFailureDetector() {
        this(DEFAULT_ALPHA);
    }
    
    @Override
    public long getIdleTime(String id, long now) {
        Monitored monitored = getMonitored(id);
        return now - monitored.lastHeard;
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
    public long getTimeToNextPing(String id, long now) {
        Monitored monitored = getMonitored(id);
        return (monitored.lastSent + monitored.eta) - now;
    }

    @Override
    public long getTimeout(String id) {
        Monitored monitored = getMonitored(id);
        return monitored.timeout;
    }
    
    @Override
    public void updatePingSample(String id, long lastHbTimestamp, 
            long interArrivalMean, long interArrivalStdDev) {
        Monitored monitored = getMonitored(id);
        
        monitored.sampWindow.clear();
        monitored.sampWindow.addInterArrival(interArrivalMean);
        updateMonitoredTimeout(lastHbTimestamp, monitored);
        monitored.lastHeard = lastHbTimestamp;
    }

    private void updateMonitoredTimeout(long now, Monitored monitored) {
        
        if (monitored.sampWindow.size() > 0) {
            Double eA = calcEA(monitored, now);
            long t = eA.longValue() + alpha;
            monitored.timeout = t - now;
        }
    }

    private Double calcEA(Monitored monitored, long now) {
        return now + monitored.sampWindow.getMean();
    }

    private Monitored getMonitored(String id) {
        Monitored monitored = monitoreds.get(id);
        if (monitored == null) {
            throw new IllegalArgumentException(
                    "The monitored must be registered first");
        }
        return monitored;
    }

    @Override
    public void messageReceived(String id, long now, MessageType type) {
        Monitored monitored = getMonitored(id);

        if (MessageType.PING.equals(type)) {
            monitored.sampWindow.addPing(now);
            updateMonitoredTimeout(now, monitored);
        }
        
        monitored.lastHeard = now;
    }

    @Override
    public void messageSent(String id, long now, MessageType type) {
        Monitored monitored = getMonitored(id);
        monitored.lastSent = now;
    }

    @Override
    public void releaseMonitored(String id) {
        monitoreds.remove(id);
    }

    @Override
    public void registerMonitored(String id, long now, long timeout) {
        Monitored monitored = monitoreds.get(id);
        if (monitored != null) {
            throw new IllegalArgumentException("Object with id " + id
                    + " is already monitored");
        }
        monitored = new Monitored(id);

        monitored.eta = timeout / 2;
        monitored.lastHeard = now;
        monitored.lastSent = now;

        monitored.timeout = timeout;

        monitoreds.put(id, monitored);

    }

    @Override
    public void setTimeout(String id, long timeout) {
        Monitored monitored = getMonitored(id);
        monitored.timeout = timeout;
        monitored.eta = timeout / 2;
    }

    private static class Monitored {

        private String id;
        InterArrivalSamplingWindow sampWindow = new InterArrivalSamplingWindow(N);
        long lastSent; //any msg sent
        long lastHeard; //any msg received

        long timeout; //dynamic timeout

        long eta; //interrogation delay

        public Monitored(String id) {
            this.id = id;
        }
    }
    
    @Override
    public void setPingInterval(String id, long interval) {
        Monitored monitored = getMonitored(id);
        monitored.eta = interval;
    }

    /**
      * Hacking function, set parameter alpha for experiment
      */
    public void setAlpha(long alpha) {
        this.alpha = alpha;
    }

    /**
      * Hacking function, get parameter alpha for experiment
      */
    public long getAlpha() {
        return alpha;
    }

    /**
     * Hacking function
     */
    public InterArrivalSamplingWindow getSampWindow(String id) {
        Monitored monitored = getMonitored(id);
        return monitored.sampWindow;
    }

    /**
     * Hacking function
     */
    public void setSampWindow(String id, InterArrivalSamplingWindow samplingWindow) {
        Monitored monitored = getMonitored(id);
        monitored.sampWindow = samplingWindow;
    }
}
