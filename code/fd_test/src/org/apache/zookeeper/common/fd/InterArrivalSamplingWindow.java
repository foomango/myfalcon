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

import java.util.LinkedList;
import java.util.List;

/**
 * Represents a sampling window for ping inter-arrivals.
 * 
 * @see FailureDetector
 */
public class InterArrivalSamplingWindow {

    private int n;
    private List<Long> samplingWindow;
    private double mean = 0;
    
    private Long lastPing;
    
    public InterArrivalSamplingWindow(int n) {
        this.n = n;
        this.samplingWindow = new LinkedList<Long>();
    }
    
    public void addPing(Long ping) {
        if (lastPing != null) {
            addInterArrival(ping - lastPing);
        }
        lastPing = ping;
    }
    
    public void addInterArrival(Long interarrival) {
        updateMean(interarrival);
        if (samplingWindow.size() == n) {
            samplingWindow.remove(0);
        }
        samplingWindow.add(interarrival);
    }

    private void updateMean(Long hb) {
        int size = samplingWindow.size();
        mean = ((double)(size) * mean + hb) / (size + 1);
    }
    
    public double getMean() {
        return mean;
    }
    
    public int size() {
        return samplingWindow.size();
    }
    
    public void clear() {
        this.samplingWindow.clear();
        this.mean = 0;
    }

    public double getStandardDeviation() {
//        double sd = 0;
//        for (long interArrival : samplingWindow) {
//            sd += Math.pow((double)(interArrival - mean),2) / samplingWindow.size();
//        }
//        return Math.sqrt(sd);
        return 0;
    }
    
    /** Hacking function
     */
    public void setSampleList(List<Long> sampleList) {
        samplingWindow = sampleList;
        double sum = 0;
        if (sampleList.size() > 0) {
            for (long sample : sampleList) {
                sum += sample;
            }
            mean = sum / sampleList.size();
        }
    }

    /** Hacking function
     */
    public List<Long> getSampleList() {
        return samplingWindow;
    }
}
