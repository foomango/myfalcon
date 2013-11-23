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
import java.nio.*;
import java.net.*;

public class FailureInduction {

    public void induceFailure(String failure) throws IOException {
        // It's very good to use function pointer here
        // But java does not have function pointer
        DatagramSocket sock = new DatagramSocket();
        DatagramPacket pkt;
        if (failure.equals("segfault")) {
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, saboteurPort);
        } else if (failure.equals("livelock")) {
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, saboteurPort);
        } else if (failure.equals("badloop")) {
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, saboteurPort);
        } else if (failure.equals("hang_spy")) {
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, saboteurPort);
        } else if (failure.equals("use_ii")) {
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, saboteurPort);
        } else if (failure.equals("badfd")) {
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, saboteurPort);
        } else if (failure.equals("handlerd")) {
            failure = "fail_handlerd";
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, failPort);
        } else if (failure.equals("os_panic")) {
            failure = "fail_panic";
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, failPort);
        } else if (failure.equals("os_exception")) {
            failure = "fail_exception";
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, failPort);
        } else if (failure.equals("os_overflow")) {
            failure = "fail_overflow";
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, failPort);
        } else if (failure.equals("os_loop")) {
            failure = "fail_loop";
            InetAddress address = InetAddress.getByName(hostName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, failPort);
        } else if (failure.equals("libvirtd")) {
            InetAddress address = InetAddress.getByName(vmmName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, failPort);
        } else if (failure.equals("hypervisor")) {
            InetAddress address = InetAddress.getByName(vmmName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, failPort);
        } else if (failure.equals("qemu")) {
            InetAddress address = InetAddress.getByName(vmmName);
            pkt = new DatagramPacket(failure.getBytes(), failure.length(),
                                     address, failPort);
        } else {
            System.out.println("Unknown failure type: " + failure + "\n");
            return;
        }
        String data = new String(pkt.getData());
        sock.send(pkt);
    }

    public FailureInduction(String hostName, String vmmName, int failPort,
            int saboteurPort) {
        this.hostName = hostName;
        this.vmmName = vmmName;
        this.failPort = failPort;
        this.saboteurPort = saboteurPort;
    }

    private String hostName;
    private String vmmName;
    private int failPort;
    private int saboteurPort;
}
