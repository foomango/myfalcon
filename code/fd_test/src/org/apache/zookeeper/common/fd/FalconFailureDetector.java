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


package org.apache.zookeeper.common.fd;

import java.io.*;
import java.lang.Long;
import java.net.*;
import java.util.HashMap;
import java.util.Map;
import java.util.Scanner;

import com.google.code.juds.*; 

public class FalconFailureDetector implements FailureDetector {

    private static final String configFile = "conf/falcon.cfg";

    private Map<String, Monitored> configMonitoreds = new HashMap<String, Monitored>();
    private Map<String, Monitored> monitoreds = new HashMap<String, Monitored>();

    public FalconFailureDetector() {

        File confFile = new File(configFile);

        Scanner scanner;
        try {
            scanner = new Scanner(new FileReader(confFile));
        }
        catch(FileNotFoundException e) {
            System.err.println(e);
            return;
        }
        try {
            while(scanner.hasNextLine()) {
                Scanner lineScanner = new Scanner(scanner.nextLine());
                lineScanner.useDelimiter(":");
                if(lineScanner.hasNext()) {
                    String id = lineScanner.next();
                    System.out.println(id);
                    String appHandle = lineScanner.next();
                    System.out.println(appHandle);
                    String hostName = lineScanner.next();
                    System.out.println(hostName);
                    String vmmName = lineScanner.next();
                    System.out.println(vmmName);
                    String switchName = lineScanner.next();
                    System.out.println(switchName);
                    int queryFrequency = Integer.parseInt(lineScanner.next().trim());
                    Monitored monitored = 
                        new Monitored(id, appHandle, hostName, vmmName, switchName, queryFrequency);

                    configMonitoreds.put(id, monitored);
                }
            }
        }
        finally {
            scanner.close();
        }
    }
    public FalconFailureDetector(Map<String, String> parameters) throws FileNotFoundException{
        this();
    }
    /**
     * Signal the failure detector of a message reception.
     * @param id the monitored object identifier
     * @param now the timestamp in which the message was received
     * @param type the type of the received message
     */
    @Override
        public void messageReceived(String id, long now, MessageType type) {
        }

    /**
     * Signal the failure detector of a message dispatch
     * @param id the monitored object identifier
     * @param now the timestamp in which the message was sent
     * @param type the type of the received message
     */
    @Override
        public void messageSent(String id, long now, MessageType type) {
        }

    /**
     * Sets the timeout of a monitored object
     * @param id the monitored object identifier
     * @param timeout the timeout for the monitored object
     */
    @Override
        public void setTimeout(String id, long timeout) {
        }

    /**
     * Appends a ping sample data to this failure detector. 
     * In ZooKeeper, this is used when Learners report client pings
     * to the Leader.
     * @param id the monitored object identifier
     * @param lastPingTimestamp 
     *          the timestamp in which the last ping was received
     * @param interArrivalMean
     *          the mean of the reported ping interarrival sample 
     * @param interArrivalStdDev
     *          the standard deviation of the reported ping 
     *          interarrival sample
     */
    @Override
        public void updatePingSample(String id, long lastPingTimestamp, long interArrivalMean,
                long interArrivalStdDev) {
        }

    /**
     * Registers an object to be monitored by this failure detector.
     * @param id the monitored object identifier, must be unique
     * @param now the timestamp in which the object 
     *          started being monitored
     * @param timeout the timeout for the monitored object
     */
    @Override
        public void registerMonitored(String id, long now, long timeout) {
            Monitored monitored = monitoreds.get(id);
            if (monitored != null) {
                //throw new IllegalArgumentException("Object with id " + id
                //        + " is already monitored");
                return;
            }
            monitored = configMonitoreds.get(id); 
            if (monitored == null) {
                throw new IllegalArgumentException("Object with id " + id
                        + " is not found in config file");
            }
            System.out.println("connect to pfd: " + monitored);
            monitored.startQuery();
            monitoreds.put(id, monitored);
        }

    /**
     * Removes the interest on a monitored object
     * @param id the monitored object identifier
     */
    @Override
        public void releaseMonitored(String id) {
            Monitored monitored = monitoreds.get(id);
            if (monitored == null) {
                return;
            }
            monitored.stopQuery();
            monitoreds.remove(id);
        }

    /**
     * Checks whether a monitored object is failed
     * @param id the monitored object identifier
     * @param now the query timestamp
     * @return true if the monitored object is failed, false otherwise
     */
    @Override
        public boolean isFailed(String id, long now) {
            //System.out.println("test id: " + id);
            Monitored monitored = monitoreds.get(id);
            if (monitored == null) {
                return false;
            }
            return monitored.isFailed();
        }

    /**
     * Checks whether a monitored object must be pinged.
     * @param id the monitored object identifier
     * @param now the query timestamp
     * @return true if the monitored object should be pinged, false otherwise
     */
    @Override
        public boolean shouldPing(String id, long now) {
            return false;
        }

    /**
     * Sets the ping interval for a monitored object
     * @param id the monitored object identifier
     * @param interval the ping interval
     */
    @Override
        public void setPingInterval(String id, long interval) {
        }

    /**
     * Retrieves the interval between now and the last time a ping was
     * received for this monitored object.
     * @param id the monitored object identifier
     * @param now the query timestamp
     * @return the idle time for the monitored object
     */
    @Override
        public long getIdleTime(String id, long now) {
            return 0;
        }

    /**
     * Retrieves the remaining time to the sending time of next ping.
     * @param id the monitored object identifier
     * @param now the query timestamp
     * @return the time remaining to the next ping
     */
    @Override
        public long getTimeToNextPing(String id, long now) {
            return Long.MAX_VALUE; 
        }

    /**
     * Retrieves the timeout of a monitored object
     * @param id the monitored object identifier
     * @return the timeout of a monitored object
     */
    @Override
        public long getTimeout(String id) {
            return Long.MAX_VALUE;
        }
    private static class Monitored {

        String id;
        String appHandle;
        String hostName;
        String vmmName;
        String switchName;
        long queryFrequency;

        boolean failed;

        QueryThread queryThread;

        public Monitored(String id, String appHandle, String hostName, 
                String vmmName, String switchName, int queryFrequency) {
            this.id = id;
            this.appHandle = appHandle;
            this.hostName = hostName;
            this.vmmName = vmmName;
            this.switchName = switchName;
            this.queryFrequency = queryFrequency;

            failed = false;
        }
        public long getFrequency() {
            return queryFrequency;
        }
        public void startQuery() {
            queryThread = new QueryThread(this);
            queryThread.start();
        }
        public void stopQuery() {
            queryThread.close();
            try {
                queryThread.join();
            }
            catch(InterruptedException e) {
            }
        }
        public boolean isFailed() {
            return failed;
        }
        public synchronized void setFailed(boolean v) {
            failed = v;
        }
        public String getId() {
            return id;
        }
        public String getAppHandle() {
            return appHandle;
        }
        public String getHostName() {
            return hostName;
        }
        public String getVmmName() {
            return vmmName;
        }
        public String getSwitchName() {
            return switchName;
        }
        public String toString() {
            String str;
            str = "app: " + appHandle + " ";
            str += "hostName: " + hostName + " ";
            str += "vmmName: " + vmmName + " ";
            str += "switchName: " + switchName + " ";
            return str;
        }
    }
    private static class QueryThread extends Thread {
        Monitored monitored;
        long queryFrequency;
        boolean isRunning;
        public QueryThread(Monitored monitored) {
            this.monitored = monitored;
            queryFrequency = monitored.getFrequency();
            isRunning = true;
        }
        public void close() {
            isRunning = false;
        }
        public void run() {
            // send handshake msg
            Socket socket;
            try {

                socket = new Socket(InetAddress.getByName("localhost"), 9090);
                System.out.println("connect to pfd server");

                PfdServerHandshake handshake = 
                    new PfdServerHandshake(monitored.getAppHandle(), monitored.getHostName(),
                        monitored.getVmmName(), monitored.getSwitchName());

                OutputStream out = socket.getOutputStream();
                InputStream in = socket.getInputStream();

                handshake.writeOutputSream(out);

                PfdProb prob = new PfdProb();
                PfdReply reply = new PfdReply();
                prob.writeOutputSream(out);

                while(isRunning) {
                    if(reply.readInputStream(in) != 0) {
                        monitored.setFailed(true);
                        break;
                    }
                    try {
                        Thread.sleep(queryFrequency);
                    }
                    catch(InterruptedException e) {
                    }
                }
                socket.close();
            }
            catch(IOException e) {
            
            }
        }
    }

    private static class PfdServerHandshake {
        private static final int kHostnameSize = 128;
        private byte[] appHandle, hostName, vmmName, switchName;  //128 * 4bytes

        public PfdServerHandshake (String app_n, String host_n, String vmm_n, String switch_n) {
            appHandle = new byte[kHostnameSize];
            hostName = new byte[kHostnameSize];
            vmmName = new byte[kHostnameSize];
            switchName = new byte[kHostnameSize];
            
            try {
                System.arraycopy(app_n.getBytes("US-ASCII"), 0, appHandle, 0, app_n.length());
                System.arraycopy(host_n.getBytes("US-ASCII"), 0, hostName, 0, host_n.length());
                System.arraycopy(vmm_n.getBytes("US-ASCII"), 0, vmmName, 0, vmm_n.length());
                System.arraycopy(switch_n.getBytes("US-ASCII"), 0, switchName, 0, switch_n.length());
            }
            catch(UnsupportedEncodingException e) {
            }
            
        }
        public void writeOutputSream(OutputStream out) throws IOException {

            byte buf[] = new byte[kHostnameSize * 4];
            System.arraycopy(appHandle, 0,  buf, 0, kHostnameSize);
            System.arraycopy(hostName, 0,  buf, kHostnameSize, kHostnameSize);
            System.arraycopy(vmmName, 0,  buf, kHostnameSize * 2, kHostnameSize);
            System.arraycopy(switchName, 0,  buf, kHostnameSize * 3, kHostnameSize);
            /*for(int i = 0; i < kHostnameSize; i++) {
              buf[i] = appHandle[i];
              }
              for(int i = 0; i < kHostnameSize; i++) {
              buf[i + kHostnameSize] = hostName[i];
              }
              for(int i = 0; i < kHostnameSize; i++) {
              buf[i + 2 * kHostnameSize] = vmmName[i];
              }
              for(int i = 0; i < kHostnameSize; i++) {
              buf[i + 3 * kHostnameSize] = switchName[i];
              }*/
            System.out.println("output handshake");
            out.write(buf, 0, kHostnameSize * 4);
            out.flush();
        }
    }

    private static class PfdReply {
        // java enum size is not as integer
        private static final int DOWN_FROM_HANDLER = 0;
        private static final int UNKNOWN_FROM_HANDLER = 1;
        private static final int DOWN_FROM_VMM = 2;
        private static final int UNKNOWN_FROM_VMM = 3;
        private static final int DOWN_FROM_SWITCH = 4;
        private static final int UNKNOWN_FROM_SWITCH = 5;
        private static final int NO_HANDLER = 6;
        private static final int NO_VMM = 7;
        private static final int NO_SWITCH = 8;
        private static final int UP = 9;
        private static final int NUM_STATUS = 10;

        static final int kStateSize = 4;

        private int state;

        public int getState() {
            return state;
        }

        public int readInputStream(InputStream in) throws IOException 
        {
            int totalBytes = 0;
            byte[] data = new byte[kStateSize];
            //byte[] buf = new byte[kStateSize];
            
            if(in.available() < kStateSize) {
                return 0;
            }
            else {
                int numBytes = in.read(data);
                return numBytes;
            }
        }
    }
    private static class PfdProb {
        private static final int kGarbageSize = 4;

        private int garbage;   // 4 bytes

        public void writeOutputSream(OutputStream out) throws IOException {

            ByteArrayOutputStream bytestream;
            bytestream = new ByteArrayOutputStream(kGarbageSize);

            DataOutputStream outstream_send = new DataOutputStream(bytestream);
            outstream_send.writeInt(garbage);

            byte garbage_buf[] = bytestream.toByteArray();
            byte[] flip_buf = new byte[kGarbageSize];
            Convertor.ReverseByteArray(garbage_buf, flip_buf, 0, kGarbageSize);

            out.write(flip_buf, 0, kGarbageSize);
            //System.out.println("garage: " + garbage);
        }
    }
    private static class Convertor {
        public static void ReverseByteArray(byte[] ori, byte[] flip, int str, int len) {
            for(int i = 0; i < len; i++) {
                flip[i] = ori[str + len - 1 - i];
            }
        }
    }

    //    public static void main(String[] args) {
    //        FalconFailureDetector fd = null;
    //        try {
    //            fd = new FalconFailureDetector();
    //        } catch(IOException e) {    
    //            System.out.println(e);
    //            return;
    //        }
    //        fd.registerMonitored("1", 0, 0);
    //        fd.registerMonitored("2", 0, 0);
    //        fd.registerMonitored("3", 0, 0);
    //        fd.isFailed("1", 0);
    //        fd.isFailed("2", 0);
    //        fd.isFailed("3", 0);
    //    }
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
