/* 
 * Copyright (c) 2011 Hao Wu, Wei-Lun Hung (University of Texas at Austin).
 * All rights reserved.
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of Texas at Austin. The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 */

package org.apache.zookeeper.test.fd;

import org.apache.zookeeper.common.fd.*;
import org.json.*;
import java.util.*;
import java.io.*;
import java.net.*;

/**
 * This class is used to monitor another application via failure detector
 * in order to test the behavior of different failure detectors.
 * @author haowu
 *
 */
public class MonitorApplication {
    /**
     * Set up a monitor application
     * @param fd the failure detector used in the monitor
     */
    public MonitorApplication(FailureDetector fd, String id, long timeout_ms, long pingInterval,
            int port, int pingTimes, FailureDetectorType fdType, boolean useTCP,
            FailureInduction failureInduction, String failureString) {
        this.id = id;
        this.fd = fd;
        this.timeout_ms = timeout_ms;
        this.pingInterval = pingInterval;
        this.pingTimes = pingTimes;
        this.fdType = fdType;
        this.failureString = failureString;
        networkManager = new NetworkManager(fd, id, port, useTCP,
                failureInduction);
    }

    public MonitorApplication(FailureDetector fd, String id, long timeout_ms, long pingInterval,
            int port, int pingTimes, FailureDetectorType fdType, boolean useTCP,
            String clientName, FailureInduction failureInduction, String failureString) {
        this.id = id;
        this.fd = fd;
        this.timeout_ms = timeout_ms;
        this.pingInterval = pingInterval;
        this.pingTimes = pingTimes;
        this.fdType = fdType;
        this.failureString = failureString;
        networkManager = new NetworkManager(fd, id, port, useTCP, clientName,
                failureInduction);
    }

    /**
     * Calculate the false detection rate and detection time using fixed
     * initial parameters.
     */
    public String calculateFixedParameters(boolean skipStablize,
            List<Long> sampleList) {
        networkManager.start();
        try {
            while (!networkManager.isReady()) {
                ; // do nothing
                // jbl: Is there no way to use a condition variable or 
                // something like that to signal that the network is ready? 
                // That seems way cleaner than me than either sleeping or 
                // polling.
                //TODO: Use conditional variable here
            }
            if (fdType == FailureDetectorType.chen ||
                fdType == FailureDetectorType.bertier ||
                fdType == FailureDetectorType.phiaccrual) {
                fd.getSampWindow(id).setSampleList(sampleList);
            }
            //XXX: The following 2 lines is for debugging purpose
            //falseRate = getFalseRate();
            //System.out.println("False Rate is " + falseRate);
            if (!skipStablize) {
                waitAndFindFalseDetection();
            }
            return testDetectionTime();
        } catch (InterruptedException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return "";
    }

    /**
     * Calculate the detection time under a fixed false detection rate.
     * @param rate false detection rate.
     * @param error the error thershold
     * @param skipStablize whether the process skips the stablization phase
     */
    public String calculateFixedFalseDetectionRate(double rate, double error,
            boolean skipStablize, List<Long> sampleList) {
        // The behavior might vary between FDs. Focus on FixedFD first
        try {
            networkManager.start();
            while (!networkManager.isReady()) {
                ;
            }
            if (fdType == FailureDetectorType.chen ||
                fdType == FailureDetectorType.bertier ||
                fdType == FailureDetectorType.phiaccrual) {
                fd.getSampWindow(id).setSampleList(sampleList);
            }

            if (skipStablize) {
                falseRate = rate;
                System.out.println("Timeout = " + fd.getTimeout(id));
            } else {
                switch (fdType) {
                    case fixed:
                        configureForFixed(rate, error);
                        break;
                    case chen:
                        configureForChen(rate, error);
                        break;
                    case bertier:
                        configureForBertier(rate, error);
                        break;
                    case phiaccrual:
                        configureForPhiAccrual(rate,error);
                        break;
                    default:
                        //TODO: Implement other fds
                }
            }
            // Test detection time
            return testDetectionTime();
        } catch (InterruptedException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return "";
    }

    /**
     * For fixed, we should configure the timeout
     */
    private void configureForFixed(double rate, double error)
        throws InterruptedException, IOException {
        FixedPingFailureDetector fixFD = (FixedPingFailureDetector) fd;
        double lowerRate = 0.0;
        long lowerTimeout = timeout_ms;
        double higherRate = 1.0;
        long higherTimeout = 2;
        double currentRate = 1.0;
        long currentTimeout = fd.getTimeout(id);
        double currentError = error + 1.0;
        long leastTimeout = timeout_ms;
        double leastError = 1.0;
        double leastRate = 1.0;
        if (fixFD != null) {
            while (higherTimeout < lowerTimeout - 1) {
                fd.setTimeout(id, currentTimeout);
                fd.setPingInterval(id, pingInterval);
                // Wait until the first message has been received
                Thread.sleep(2*timeout_ms);
                currentRate = getFalseRate();
                currentError = currentRate - rate;
                if (Math.abs(currentError) <= leastError) {
                    leastError = Math.abs(currentError);
                    leastRate = currentRate;
                    leastTimeout = currentTimeout;
                }
                if (Math.abs(currentError) < error) {
                    break;
                } else if (currentError > 0) {
                    //Rate too large, increase timeout
                    higherRate = currentRate;
                    higherTimeout = currentTimeout;
                    currentTimeout = (currentTimeout + lowerTimeout) / 2;
                } else {
                    //Rate too small, decrease timeout
                    lowerRate = currentRate;
                    lowerTimeout = currentTimeout;
                    currentTimeout = (currentTimeout + higherTimeout) / 2;
                }
                System.out.println("Current Timeout " + currentTimeout);
                System.out.println("higherTimeout " + higherTimeout);
                System.out.println("lowererTimeout " + lowerTimeout);
                System.out.println("CurrentError " + currentError);
                System.out.println("CurrentRate " + currentRate);
            }
            try {
                configObject.put("timeout", leastTimeout);
            } catch (JSONException e) {
                e.printStackTrace();
            }
            falseRate = leastRate;
            timeout_ms = leastTimeout;
            fd.setTimeout(id, leastTimeout);
            System.out.println("Timeout=" + leastTimeout);
        }
    }

    private void configureForChen(double rate, double error) throws IOException,
            InterruptedException {
        ChenFailureDetector chenFD = (ChenFailureDetector) fd;
        double currentRate = 1.0;
        long currentAlpha = chenFD.getAlpha();
        double lowerRate = 0.0;
        long lowerAlpha = currentAlpha;
        double higherRate = 1.0;
        long higherAlpha = 0;
        double currentError = error + 1.0;
        long leastAlpha = currentAlpha;
        double leastError = 1.0;
        double leastRate = 1.0;
        if (chenFD != null) {
            while (higherRate - lowerRate > error &&
                    higherAlpha < lowerAlpha - 1) {
                chenFD.setAlpha(currentAlpha);
                waitAndFindFalseDetection();
                currentRate = getFalseRate();
                currentError = currentRate - rate;
                if (Math.abs(currentError) <= leastError) {
                    leastError = Math.abs(currentError);
                    leastRate = currentRate;
                    leastAlpha = currentAlpha;
                }
                if (Math.abs(currentError) < error) {
                    break;
                } else if (currentError > 0) {
                    //Rate too large, increase Alpha
                    higherRate = currentRate;
                    higherAlpha = currentAlpha;
                    currentAlpha = (currentAlpha + lowerAlpha) / 2;
                } else {
                    //Rate too small, decrease Alpha
                    lowerRate = currentRate;
                    lowerAlpha = currentAlpha;
                    currentAlpha = (currentAlpha + higherAlpha) / 2;
                }
                System.out.println("Current Alpha " + currentAlpha);
                System.out.println("higherAlpha " + higherAlpha);
                System.out.println("lowererAlpha " + lowerAlpha);
                System.out.println("CurrentError " + currentError);
                System.out.println("CurrentRate " + currentRate);
            }
            falseRate = leastRate;
            chenFD.setAlpha(leastAlpha);
            timeout_ms = chenFD.getTimeout(id);
            waitAndFindFalseDetection();
            System.out.println("Alpha=" + leastAlpha);
            try {
                //record sampling window
                List<Long> samp = chenFD.getSampWindow(id).getSampleList();
                configObject.put("samplingwindow", samp);
                configObject.put("alpha", leastAlpha);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
    }

    //TODO: implement
    private void configureForBertier(double rate, double error) throws IOException,
            InterruptedException {
        BertierFailureDetector bertierFD = (BertierFailureDetector) fd;
        double currentRate = 1.0;
        double currentBeta = bertierFD.getBeta();
        double lowerRate = 0.0;
        double lowerBeta = currentBeta;
        double higherRate = 1.0;
        double higherBeta = 0;
        double currentError = error + 1.0;
        double leastBeta = currentBeta;
        double leastError = 1.0;
        double leastRate = 1.0;
        if (bertierFD != null) {
            while (higherRate - lowerRate > error &&
                    higherBeta < lowerBeta - 0.0001) {
                bertierFD.setBeta(currentBeta);
                waitAndFindFalseDetection();
                currentRate = getFalseRate();
                currentError = currentRate - rate;
                if (Math.abs(currentError) <= leastError) {
                    leastError = Math.abs(currentError);
                    leastRate = currentRate;
                    leastBeta = currentBeta;
                }
                if (Math.abs(currentError) < error) {
                    break;
                } else if (currentError > 0) {
                    //Rate too large, increase Beta
                    higherRate = currentRate;
                    higherBeta = currentBeta;
                    currentBeta = (currentBeta + lowerBeta) / 2;
                } else {
                    //Rate too small, decrease Beta
                    lowerRate = currentRate;
                    lowerBeta = currentBeta;
                    currentBeta = (currentBeta + higherBeta) / 2;
                }
                System.out.println("Current Beta " + currentBeta);
                System.out.println("higherBeta " + higherBeta);
                System.out.println("lowererBeta " + lowerBeta);
                System.out.println("CurrentError " + currentError);
                System.out.println("CurrentRate " + currentRate);
            }
            falseRate = leastRate;
            bertierFD.setBeta(leastBeta);
            timeout_ms = bertierFD.getTimeout(id);
            waitAndFindFalseDetection();
            System.out.println("Beta=" + leastBeta);
            try {
                //record sampling window
                List<Long> samp = bertierFD.getSampWindow(id).getSampleList();
                configObject.put("samplingwindow", samp);
                configObject.put("beta", leastBeta);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
    }

    //TODO: Implement
    private void configureForPhiAccrual(double rate, double error) throws IOException,
            InterruptedException {
        PhiAccrualFailureDetector phiAccrualFD = (PhiAccrualFailureDetector) fd;
        double currentRate = 1.0;
        double currentThreshold = phiAccrualFD.getThreshold();
        double lowerRate = 0.0;
        double lowerThreshold = currentThreshold;
        double higherRate = 1.0;
        double higherThreshold = 0.0;
        double currentError = error + 1.0;
        double leastThreshold = currentThreshold;
        double leastError = 1.0;
        double leastRate = 1.0;
        // Wait until min window size has been reached
        int minWindowSize = phiAccrualFD.getMinWindowSize();
        System.out.println("Waiting for min window size " + minWindowSize);
        Thread.sleep(minWindowSize * pingInterval);
        System.out.println("Waiting for min window size done!");
        if (phiAccrualFD != null) {
            while (higherRate - lowerRate > error &&
                    higherThreshold < lowerThreshold - 0.0001) {
                phiAccrualFD.setThreshold(currentThreshold);
                waitAndFindFalseDetection();
                currentRate = getFalseRate();
                currentError = currentRate - rate;
                if (Math.abs(currentError) <= leastError) {
                    leastError = Math.abs(currentError);
                    leastRate = currentRate;
                    leastThreshold = currentThreshold;
                }
                if (Math.abs(currentError) < error) {
                    break;
                } else if (currentError > 0) {
                    //Rate too large, increase Threshold
                    higherRate = currentRate;
                    higherThreshold = currentThreshold;
                    currentThreshold = (currentThreshold + lowerThreshold) / 2;
                } else {
                    //Rate too small, decrease Threshold
                    lowerRate = currentRate;
                    lowerThreshold = currentThreshold;
                    currentThreshold = (currentThreshold + higherThreshold) / 2;
                }
                System.out.println("Current Threshold " + currentThreshold);
                System.out.println("higherThreshold " + higherThreshold);
                System.out.println("lowererThreshold " + lowerThreshold);
                System.out.println("CurrentError " + currentError);
                System.out.println("CurrentRate " + currentRate);
            }
            falseRate = leastRate;
            phiAccrualFD.setThreshold(leastThreshold);
            timeout_ms = phiAccrualFD.getTimeout(id);
            waitAndFindFalseDetection();
            System.out.println("Threshold=" + leastThreshold);
            try {
                //record sampling window
                List<Long> samp = phiAccrualFD.getSampWindow(id).getSampleList();
                configObject.put("samplingwindow", samp);
                configObject.put("threshold", leastThreshold);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
    }

    private String testDetectionTime() throws IOException, InterruptedException {
        if (fdType == FailureDetectorType.falcon) {
            Thread.sleep((long)(Math.random() * 100));
        } else {
            Thread.sleep((long)(Math.random()*pingInterval));
        }
        System.out.println("Timeout is " + timeout_ms);
        startTime = new Date().getTime();
        System.out.println("Start Time is " + startTime);
        try {
            induceFailure();
        } catch (IOException e) {
            e.printStackTrace();
        }
        induceTime = new Date().getTime();
        System.out.println("Induce Time is " + induceTime);
        // jbl: Will this poll? Don't some of the FDs take into account 
        // protocol messages? If that's the case, where is that being taken
        // into account?
        // hw: I didn't see any Fds using protocol messages
        // jl: A whole bunch of them use non-Ping messages to update last 
        // heard, which is state they use to calculate IsFailed(). There is
        // then a tradeoff: falcon can't exactly use thos messages right 
        // now, and that is a disadvantage we need to examine. Can you think
        // of any way to highlight that in the evaluation?
        // hw: Introduce E2E timeout here
        while (!fd.isFailed(id, new Date().getTime())) {
            // Just wait until fd detects the failure
            Thread.sleep(1);
            if (fdType == FailureDetectorType.falcon &&
                    new Date().getTime() - induceTime > timeout_ms) {
                e2eTimeoutObserved = true;
                break;
            }
        }
        detectTime = new Date().getTime();
        System.out.println("Detect Time is " + detectTime);
        return output();
    }

    private double getFalseRate() throws IOException,
            InterruptedException {
        //TODO: Less magic
        int falseCount = 0;
        //int totalCount = 10000;
        long fdTimeout = fd.getTimeout(id);
        int totalCount;
        if (fdTimeout < 10000) {
            totalCount = (int)((10000 / fdTimeout) * fdTimeout);
        } else if (fdTimeout < 20000) {
            totalCount = (int)fdTimeout;
        } else {
            totalCount = 100;
        }
        for (int i = 0; i < totalCount; i++) {
            if (fd.isFailed(id, new Date().getTime())) {
                falseCount ++;
            }
            Thread.sleep(1);
        }
        return (double)falseCount / totalCount;
    }

    private void induceFailure() throws IOException,
            InterruptedException {
        if (failureString != "no_failure") {
            networkManager.induceFailure(failureString);
        }
    }

    private String output() {
        StringBuilder sb = new StringBuilder();
        sb.append(networkManager.getNetworkCost());
        sb.append(" ");
        sb.append(induceTime - startTime);
        sb.append(" ");
        sb.append(detectTime - induceTime);
        sb.append(" ");
        sb.append(falseRate);
        sb.append(" ");
        sb.append(fd.getTimeout(id));
        //sb.append(detectTime);
        if (fdType == FailureDetectorType.falcon) {
            sb.append(" ");
            sb.append(e2eTimeoutObserved);
        }
        sb.append("\n");
        return sb.toString();
    }

    /**
     *  Used to get config for next iteration.
     *  e.g: Timeout, Rate, Alpha, Phi, etc
     */
    public JSONObject getConfig() {
        try {
            configObject.put("timeout", timeout_ms);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return configObject;
    }

    private void waitAndFindFalseDetection() throws InterruptedException {
        long openTime = new Date().getTime();
        long currentTime = new Date().getTime();
        while (!networkManager.isStable() && currentTime - openTime < pingTimes * timeout_ms) {
            if (fd.isFailed(id, currentTime)) {
                falseDetection = true;
            }
            Thread.sleep(pingInterval);
            currentTime = new Date().getTime();
        }
    }

    private FailureDetector fd;
    private String id;
    private long timeout_ms, pingInterval;
    private long startTime, induceTime, detectTime;
    private NetworkManager networkManager;
    private int pingTimes;
    private boolean falseDetection = false;
    private boolean e2eTimeoutObserved = false;
    private double falseRate;
    private FailureDetectorType fdType;
    private JSONObject configObject = new JSONObject();
    private String failureString;
    private enum FailureDetectorType {
        fixed,
        chen,
        bertier,
        phiaccrual,
        falcon
    }

    /**
     * Manager communication as a server
     * Only communicate with one application now
     * @author haowu
     *
     */
    public class NetworkManager extends Thread {
        public NetworkManager(FailureDetector fd, String id, int port,
                boolean useTCP, FailureInduction failureInduction) {
            this.fd = fd;
            this.id = id;
            this.port = port;
            this.useTCP = useTCP;
            this.failureInduction = failureInduction;
        }

        public NetworkManager(FailureDetector fd, String id, int port,
                boolean useTCP, String clientName,
                FailureInduction failureInduction) {
            this.fd = fd;
            this.id = id;
            this.port = port;
            this.useTCP = useTCP;
            this.clientName = clientName;
            this.failureInduction = failureInduction;
        }

        public boolean isReady() {
            boolean result;
            synchronized(ready) {
                result = ready;
            }
            return result;
        }

        public long getNetworkCost() {
            int result;
            result = msgCount;
            return result;
        }

        public boolean isStable() {
            // Currently, the fd is stable when 3 past timeout values
            // are equal.
            synchronized (timeouts) {
                int size = timeouts.size();
                if (size < 3) {
                    return false;
                } else if ((timeouts.get(size - 1) == timeouts.get(size - 2)) &&
                        (timeouts.get(size - 1) == timeouts.get(size - 3))) {
                    return true;
                } else {
                    return false;
                }
            }
        }

        // jbl: Hao, please use the failure injection framework to inject
        // failures. Register a failure in induce_failure.cc and use that to
        // inject failures. This contributes to a coherent methodology for the
        // evaluation. You'll need to create a saboteur thread (which isn't that
        // difficult, look in libfail for ideas).
        public void induceFailure(String failString) throws IOException,
               InterruptedException {
            failureInduction.induceFailure(failString);
        }
        /*public void induceFailure() throws InterruptedException {
            try {
                String message = "Fail App";
                synchronized (msg) {
                    message = "Fail App";
                }
                if (useTCP) {
                    out.println(message);
                } else {
                    sentThread.sendMessage(message);
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }*/

        /*private class ReceiveMessageThread extends Thread {
            public ReceiveMessageThread() {
            }

            @Override
            public void run() {
                while (true) {
                    try {
                        Date sentTime;
                        synchronized (msg) {
                            sentTime = new Date();
                            fd.messageSent(id, sentTime.getTime(), MessageType.PING);
                            out.println(msg);
                        }
                        synchronized(msgCount) {
                            msgCount = msgCount + 1;
                        }
                        String status = in.readLine();
                        receivedTime = new Date();
                        //System.out.println("Sent Time is " + sentTime.getTime());
                        //System.out.println("Receive Time is " + receivedTime.getTime());
                        synchronized (timeouts) {
                            timeouts.add(fd.getTimeout(id));
                        }
                        // jbl: Actually. Why is the monitoring thread even 
                        // sending pings? Shouldn't it only send protocol messages,
                        // and receive pings (/protocol messages)?
                        // hw: They won't. I leave it here in case they will act like that
                        // in the future.
                        if (status == "Dead") {
                            terminate = true;
                        } else {
                            fd.messageReceived(id, receivedTime.getTime(), MessageType.PING);
                        }
                        long elapseTime = fd.getTimeToNextPing(id, new Date().getTime());
                        if (elapseTime > 0) {
                            Thread.sleep(elapseTime);
                        }
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
        }*/

        @Override
        public void run() {
            if (useTCP) {
                runTCP();
            } else {
                runUDP();
            }
        }

        private class SentPingViaUDPThread extends Thread {
            public SentPingViaUDPThread(DatagramSocket socket,
                    InetAddress address, int port) {
                this.socket = socket;
                this.address = address;
                this.port = port;
            }

            @Override
            public void run() {
                while (true) {
                    try {
                        String message;
                        synchronized (msg) {
                            message = msg;
                            //System.out.println("Message: " + msg);
                        }
                        sendMessage(message);
                        Date sentTime = new Date();
                        fd.messageSent(id, sentTime.getTime(), MessageType.PING);
                        long elapseTime = fd.getTimeToNextPing(id, new Date().getTime());
                        if (elapseTime > 0) {
                            Thread.sleep(elapseTime);
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }

            public synchronized void sendMessage(String message) throws IOException {
                byte[] data;
                data = message.getBytes();
                DatagramPacket packet =
                    new DatagramPacket(data, data.length, address, port);
                socket.send(packet);
            }

            private DatagramSocket socket;
            private InetAddress address;
            private int port;
        }

        public void runUDP() {
            try {
                fd.registerMonitored(id, new Date().getTime(), timeout_ms);
                fd.setPingInterval(id, pingInterval);
                DatagramSocket serverSocket = new DatagramSocket(port);
                InetAddress clientAddress = InetAddress.getByName(clientName);
                sentThread = new SentPingViaUDPThread(
                        serverSocket, clientAddress, port);
                sentThread.start();

                while(!terminate)
                {
                    byte[] statusData = new byte[1024];
                    DatagramPacket packet = new DatagramPacket(
                            statusData, statusData.length);
                    serverSocket.receive(packet);
                    synchronized (ready) {
                        ready = true;
                    }
                    String status = new String(packet.getData());
                    //System.out.println("Status: " + status);
                    Date receivedTime = new Date();
                    synchronized (timeouts) {
                        timeouts.add(fd.getTimeout(id));
                    }
                    if (status == "Dead") {
                        terminate = true;
                    } else {
                        fd.messageReceived(id, receivedTime.getTime(), MessageType.PING);
                    }
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        public void runTCP() {
            ServerSocket server = null;
            Socket socket = null;
            try {
                server = new ServerSocket(port);
                socket = server.accept();
                System.out.println("Accept successfully!");
                in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                out = new PrintWriter(socket.getOutputStream(), true);
                fd.registerMonitored(id, new Date().getTime(), timeout_ms);
                if (fdType == FailureDetectorType.falcon) {
                    Thread.sleep(4000);
                }
                fd.setPingInterval(id, pingInterval);
                while (!terminate) {
                    Date sentTime, receivedTime;
                    synchronized (msg) {
                        sentTime = new Date();
                        fd.messageSent(id, sentTime.getTime(), MessageType.PING);
                        out.println(msg);
                    }
                    synchronized(msgCount) {
                        msgCount = msgCount + 1;
                    }
                    String status = in.readLine();
                    receivedTime = new Date();
                    //System.out.println("Sent Time is " + sentTime.getTime());
                    //System.out.println("Receive Time is " + receivedTime.getTime());
                    synchronized (timeouts) {
                        timeouts.add(fd.getTimeout(id));
                    }
                    // jbl: Actually. Why is the monitoring thread even 
                    // sending pings? Shouldn't it only send protocol messages,
                    // and receive pings (/protocol messages)?
                    // hw: They won't. I leave it here in case they will act like that
                    // in the future.
                    if (status == null) {
                        terminate = true;
                    } else {
                        fd.messageReceived(id, receivedTime.getTime(), MessageType.PING);
                        System.out.println("Current Timeout is " + fd.getTimeout(id));
                    }
                    // Need to update lastHeard here
                    synchronized (ready) {
                        ready = true;
                    }
                    long elapseTime = fd.getTimeToNextPing(id, new Date().getTime());
                    if (elapseTime > 0) {
                        Thread.sleep(elapseTime);
                    }
                }
            } catch (IOException e) {
                e.printStackTrace();
            } catch (InterruptedException e) {
                e.printStackTrace();
            } finally {
                try {
                    server.close();
                    socket.close();
                    in.close();
                    out.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        private int port;
        private BufferedReader in;
        private PrintWriter out;
        private boolean terminate = false;
        private String msg = "Ping";
        private FailureDetector fd;
        private String id;
        private Integer msgCount = 0;
        private Boolean ready = false;
        List<Long> intervals = new ArrayList<Long>();
        List<Long> timeouts = new ArrayList<Long>();
        private boolean useTCP = true;
        private String clientName;
        SentPingViaUDPThread sentThread;
        private FailureInduction failureInduction;
    }

    public static void main(String args[]) {
        try {
            BufferedReader parameterFileReader =
                new BufferedReader(new FileReader(args[0]));
            StringBuilder parameterStringBuilder = new StringBuilder();
            String line;
            while ((line = parameterFileReader.readLine()) != null) {
                parameterStringBuilder.append(line);
            }
            String parameterString = parameterStringBuilder.toString();
            JSONObject parameters = new JSONObject(parameterString);
            String fdName = parameters.getString("fd_name");
            long timeout_ms = parameters.getLong("timeout");
            int pingTimes = parameters.getInt("ping_times");
            int port = parameters.getInt("port");
            String id = parameters.getString("app_id");
            String data_file = parameters.getString("output_file");
            JSONObject propertiesJSONObject = parameters.getJSONObject("properties");
            boolean fixedDetectionRate = parameters.getBoolean("fixed_rate");
            boolean fileAppend = parameters.getBoolean("append");
            boolean useTCP = parameters.getBoolean("tcp");
            String failureString = parameters.getString("failure");
            long pingInterval = parameters.getLong("ping_interval");
            boolean skipStablization = parameters.getBoolean("skip_stablization");

            Map<String, String> properties = new TreeMap<String, String>();
            for (Iterator prop_it = propertiesJSONObject.keys();
                    prop_it.hasNext(); ) {
                String key = prop_it.next().toString();
                String value = propertiesJSONObject.getString(key);
                properties.put(key, value);
            }
            FailureDetectorFactory fdFactory = new FailureDetectorFactory();
            FailureDetector fd = fdFactory.createFd(fdName, null, properties);
            // Update sampling window if there is any
            List<Long> sampleList = new LinkedList<Long>();
            if (propertiesJSONObject.has("samplingwindow")) {
                JSONArray sampleJSONArray = propertiesJSONObject.getJSONArray("samplingwindow");
                for (int i = 0; i < sampleJSONArray.length(); i++) {
                    sampleList.add(sampleJSONArray.getLong(i));
                }
            }
            // Create failure inductor
            String hostName = parameters.getString("host_name");
            String vmmName = parameters.getString("vmm_name");
            int failPort = parameters.getInt("fail_port");
            int handlerdPort = parameters.getInt("handlerd_port");
            FailureInduction failureInduction = new FailureInduction(
                   hostName, vmmName, failPort, handlerdPort);
            MonitorApplication app;
            if (useTCP) {
                app = new MonitorApplication(fd, id, timeout_ms, pingInterval,
                        port, pingTimes, (FailureDetectorType.valueOf(fdName)),
                        useTCP, failureInduction, failureString);
            } else {
                String clientName = parameters.getString("address");
                app = new MonitorApplication(fd, id, timeout_ms, pingInterval,
                        port, pingTimes, (FailureDetectorType.valueOf(fdName)),
                        useTCP, clientName, failureInduction, failureString);
            }
            String data, config_file;
            JSONObject config;
            config_file = parameters.getString("config_file");
            if (fixedDetectionRate) {
                double rate = parameters.getDouble("false_rate");
                double error = parameters.getDouble("false_rate_error");
                data = app.calculateFixedFalseDetectionRate(rate, error, skipStablization, sampleList);
            } else {
                data = app.calculateFixedParameters(skipStablization, sampleList);
            }
            BufferedWriter dataWriter = new BufferedWriter(new FileWriter(data_file, fileAppend));
            dataWriter.write(data);
            dataWriter.close();
            config = app.getConfig();
            dataWriter = new BufferedWriter(new FileWriter(config_file, false));
            dataWriter.write(config.toString());
            dataWriter.close();
            System.out.println("System Terminating");
            System.exit(0);
        } catch (Exception e) {
            e.printStackTrace();
            System.exit(-1);
        }
    }
}

