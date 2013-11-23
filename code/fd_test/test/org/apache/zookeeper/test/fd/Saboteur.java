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
import java.io.*;
import java.net.*;

/**
 *  This class is used to simulate a saboteur in code/src/libfail/saboteur.cc
 *  @author haowu
 */
public class Saboteur extends Thread {
    public Saboteur(int port, FalconSpy spy) {
        this.port = port;
        this.spy = spy;
    }

    @Override
    public void run() {
        try {
            DatagramSocket serverSocket = null;
            serverSocket = new DatagramSocket(port);
            byte [] buf = new byte[1024];
            DatagramPacket pkt = new DatagramPacket(buf, 1024);
            serverSocket.receive(pkt);
            String status = new String(pkt.getData(), 0, pkt.getLength());
            System.out.println("Got a failure: " + status);
            causeFailure(status);
            System.out.println("Still Alive? Something must be wrong here!");
        } catch (IOException e) {
            // This should happen only if socket fails
            e.printStackTrace();
        } catch (Exception e) {
            System.exit(0);
        }
    }

    /**
     *  Cause a failure according to its type.
     *  XXX: Currently we only support segmentation fault.
     */
    private void causeFailure(String status) {
        // Throws a NullPointerException
        // Used to simulate segmentation fault.
        if (status.equals("segfault")) {
            int[] x = null;
            x[0] = 11;
            System.out.print("Shouldn't display this! x[0]=" + x[0] + "\n");
        } else if (status.equals("spy_hang")) {
            if (spy != null) {
                spy.setHang();
            }
        } else if (status.equals("use_ii")) {
            if (spy != null) {
                spy.setFail();
            }
        } else if (status.equals("badfd")) {
            try {
                FileWriter fstream = new FileWriter("/proc/badfd");
                BufferedWriter out = new BufferedWriter(fstream);
                out.write("1");
                out.close();
            } catch (Exception e) {
                System.err.println("Error: " + e.getMessage());
            }
            for (;;);
        }
        System.out.print("segfault\n" + status + "\n");
        System.out.print("segfault".length() + "\n");
        System.out.print(status.length() + "\n");
    }

    private int port;
    private FalconSpy spy;
}
