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

import java.util.Iterator;
import java.util.Map;
import java.util.TreeMap;
import java.io.*;
import java.net.*;

import org.apache.zookeeper.common.fd.*;
import org.json.*;

/**
 * An application which does nothing
 * @author haowu
 *
 */
public class DummyApplication {
    public DummyApplication(String addr, int port, int timeout_ms,
            boolean useTCP, int saboteurPort, boolean startSpy) {
        networkManager = new NetworkManager(addr, port, timeout_ms, useTCP);
        this.startSpy = startSpy;
        if (startSpy) {
            spy = new FalconSpy();
        }
        saboteur = new Saboteur(saboteurPort, spy);
    }

    /**
     * Do nothing - just start the networkManager
     */
    public void run() {
        networkManager.start();
        saboteur.start();
        if (startSpy) {
            spy.start();
        }
    }

    /**
     * Manage communication as a client
     * @author haowu
     * 
     */
    public class NetworkManager extends Thread {
        public NetworkManager(String addr, int port, int timeout_ms, boolean useTCP) {
            this.address = addr;
            this.port = port;
            this.timeout_ms = timeout_ms;
            this.useTCP = useTCP;
        }

        @Override
        public void run() {
            if (useTCP) {
                runTCP();
            } else {
                runUDP();
            }
        }

        private void runTCP() {
            try {
                System.out.println("Address = " + address + "\t Port = " + port);
                socket = new Socket(address, port);
                in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                out = new PrintWriter(socket.getOutputStream(), true);
                while (running) {
                    String msg = in.readLine();
                    if (!saboteur.isAlive()) {
                        running = false;
                        break;
                    }
                    out.println("Alive");
                }
            } catch(IOException e) {
                e.printStackTrace();
            } finally {
                try {
                    in.close();
                    out.close();
                    socket.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }

        private void runUDP() {
            try {
                clientSocket = new DatagramSocket(port);
                while (running) {
                    byte[] data = new byte[1024];
                    DatagramPacket packet = new DatagramPacket(data, data.length);
                    clientSocket.receive(packet);
                    String msg = new String(packet.getData());
                    //System.out.println(msg);
                    if (!saboteur.isAlive()) {
                        running = false;
                    }
                    data = "Alive".getBytes();
                    InetAddress fdAddress = packet.getAddress();
                    int fdPort = packet.getPort();
                    packet = new DatagramPacket(data, data.length,
                            fdAddress, fdPort);
                    clientSocket.send(packet);
                }
            } catch(IOException e) {
                e.printStackTrace();
            } finally {
                clientSocket.close();
            }
        }

        /**
         * Deal with incoming message from network
         * @param str the incoming message
         */
        // jbl: see note in MonitorApplication.java about failure injection
        // framework.
        private void dealWithMsg(String str) {
            if (str.startsWith("Fail")) {
                alive = false;
            } else if (str.startsWith("Terminate")) {
                alive = false;
                running = false;
            }
        }

        private String address;
        private int port;
        private boolean alive = true;
        private boolean running = true;
        private Socket socket;
        private DatagramSocket clientSocket;
        private BufferedReader in;
        private PrintWriter out;
        private int timeout_ms;
        private boolean useTCP;
    }

    private NetworkManager networkManager;
    private Saboteur saboteur;
    private FalconSpy spy;
    private boolean startSpy;

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
            String addr = parameters.getString("address");
            int port = parameters.getInt("port");
            int timeout_ms = parameters.getInt("timeout");
            boolean useTCP = parameters.getBoolean("tcp");
            int saboteurPort = parameters.getInt("saboteur_port");
            boolean startSpy = parameters.getBoolean("start_spy");
            DummyApplication app = new DummyApplication(addr, port, timeout_ms, useTCP, saboteurPort,
                    startSpy);
            app.run();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
