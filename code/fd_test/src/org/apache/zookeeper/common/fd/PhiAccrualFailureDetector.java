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
 * Failure detector implementation according
 * to the Phi Accrual method, as described in the paper
 * 'The Phi Accrual Failure Detector', by Hayashibara. 
 * This method estimates a phi value from the ping arrival 
 * time sampling window. This phi value indicates a suspicion 
 * level of a certain object. If the phi value exceeds a 
 * threshold defined by the application, the monitored object 
 * is assumed as failed.
 * 
 * An exponential distribution is used to model
 * the sampling window, as described in the article 
 * 'ED FD: Improving the Phi Accrual Failure Detector'. 
 * @see PhiTimeoutEvaluator
 * 
 */
public class PhiAccrualFailureDetector implements FailureDetector {

    protected static final int DEFAULT_MINWINDOWSIZE = 500;
    protected static final double DEFAULT_THRESHOLD = 2.;
    
    private static final int N = 1000;
    private Map<String, Monitored> monitoreds = new HashMap<String, Monitored>();

    private double threshold;
    private final int minWindowSize;

    /**
     * Create a PhiAccrualFailureDetector with default threshold value (2.)  
     * and default minWindowSize (500).
     */
    public PhiAccrualFailureDetector() {
        this(DEFAULT_THRESHOLD, DEFAULT_MINWINDOWSIZE);
    }

    /**
     * Create a PhiAccrualFailureDetector with specified threshold.
     * 
     * @param threshold
     *            for the phi value. When the phi value exceeds this threshold
     *            for a certain monitored, the failure detector considers this
     *            object as failed.
     * @param minWindowSize
     *            the sampling window minimum size for the failure detector to
     *            become active. This lower bound gives the failure detector a
     *            warm-up period.
     */
    public PhiAccrualFailureDetector(double threshold, int minWindowSize) {
        this.threshold = threshold;
        this.minWindowSize = minWindowSize;
    }
    
    /**
     * Create a PhiAccrualFailureDetector from a parameters map
     * @param parameters
     */
    public PhiAccrualFailureDetector(Map<String, String> parameters) {
        this(
                FailureDetectorOptParser.parseDouble(
                        PhiAccrualFailureDetector.DEFAULT_THRESHOLD, 
                        parameters.get("threshold")), 
                FailureDetectorOptParser.parseInt(
                        PhiAccrualFailureDetector.DEFAULT_MINWINDOWSIZE, 
                        parameters.get("minwindowsize")));
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
    public void setPingInterval(String id, long interval) {
        Monitored monitored = getMonitored(id);
        monitored.eta = interval;
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

    private void updateTimeout(Monitored monitored) {
        long mean = (long) monitored.sampWindow.getMean();
        monitored.timeout = (long) (
                -Math.log(Math.pow(10, -threshold)) * mean);
    }

    @Override
    public void updatePingSample(String id, long lastHbTimestamp,
            long interArrivalMean, long interArrivalStdDev) {
        
        Monitored monitored = getMonitored(id);
        monitored.lastHeard = lastHbTimestamp;
        
        monitored.sampWindow.clear();
        monitored.sampWindow.addInterArrival(interArrivalMean);
        
        updateTimeout(monitored);
    }

    @Override
    public void releaseMonitored(String id) {
        monitoreds.remove(id);
    }

    @Override
    public void setTimeout(String id, long timeout) {
        Monitored monitored = getMonitored(id);
        monitored.eta = timeout / 2;
        monitored.timeout = timeout;
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
    public void registerMonitored(String id, long now, long timeout) {
        Monitored monitored = monitoreds.get(id);
        if (monitored != null) {
            throw new IllegalArgumentException("Object with id " + id
                    + " is already monitored");
        }
        monitored = new Monitored(id);
        monitored.timeout = timeout;

        monitored.lastHeard = now;
        monitored.lastSent = now;
        monitored.eta = timeout / 2;

        monitoreds.put(id, monitored);
    }

    private static class Monitored {

        String id;
        InterArrivalSamplingWindow sampWindow = new InterArrivalSamplingWindow(N);
        
        long lastSent; 
        long lastHeard;

        long timeout; //dynamic timeout

        long eta; //interval between pings

        public Monitored(String id) {
            this.id = id;
        }

    }

    @Override
    public void messageReceived(String id, long now, MessageType type) {
        Monitored monitored = getMonitored(id);
        
        if (MessageType.PING.equals(type)) {
            monitored.sampWindow.addPing(now);
            if (monitored.sampWindow.size() > minWindowSize) {
                updateTimeout(monitored);
            }
        }
        
        monitored.lastHeard = now;
    }

    @Override
    public void messageSent(String id, long now, MessageType type) {
        Monitored monitored = getMonitored(id);
        monitored.lastSent = now;
    }

    /**
     *  Hacking function for experiment
     *  @author haowu
     */
    public int getMinWindowSize() {
        return minWindowSize;
    }

    /**
     *  Hacking function for experiment
     *  @author haowu
     */
    public double getThreshold() {
        return threshold;
    }

    /**
     *  Hacking function for experiment
     *  @author haowu
     */
    public void setThreshold(double value) {
        threshold = value;
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
