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

import java.lang.management.ManagementFactory;
import java.lang.Long;
import java.io.*;

import com.google.code.juds.*; 


public class FalconSpy extends Thread {
    private static final String socketFile = "/tmp/falcon";

    HandlerdReply reply;
    public void run() {
        UnixDomainSocketClient socket = null;
        try {
            socket = new UnixDomainSocketClient(socketFile,
                    UnixDomainSocket.SOCK_STREAM);
            InputStream in = socket.getInputStream();
            OutputStream out = socket.getOutputStream();

            // send handshake
            HandlerdHandshake handshake = new HandlerdHandshake("zookeeper");
            handshake.writeToOutStream(out);

            System.out.println("send handlerd handshake\n");
            SpyProb prob = new SpyProb();
            reply = new HandlerdReply();
            for(;;) {
                prob.readFromInputStream(in);
                reply.writeToOutStream(out);
                //System.out.println("send alive to handlerd\n");
            }
        }
        catch (Exception e) {
            System.out.println("Exception in FalconSpy");
            e.printStackTrace();
            if(socket != null) {
                socket.close();
            }
        }
    }

    public void setHang() {
        reply.setHang();
    }

    public void setFail() {
        reply.setFail();
    }

    private static class HandlerdHandshake {
        private static final int kHandleSize = 32;
        private static final int kPidSize = 4;
        private byte[] handle;  //32bytes
        private int pid;       //32bits, 4bytes
        private int delay_ms;
        private byte delay_type;

        public HandlerdHandshake(String s) {
            //Long lpid = new Long(Thread.currentThread().getId());
            String strs[] =
                ManagementFactory.getRuntimeMXBean().getName().split("@");
            //System.out.println("lpid: " + lpid);
            //pid = lpid.intValue();
            pid = Integer.parseInt(strs[0]);
            System.out.println("pid: " + pid);
            handle = new byte[kHandleSize];
            delay_ms = 100;
            delay_type = 1;

            try {
                System.arraycopy(s.getBytes("US-ASCII"), 0, handle, 0, s.length());
            }
            catch (UnsupportedEncodingException e) {
            }
        }
        public void writeToOutStream(OutputStream out) throws IOException {
            ByteArrayOutputStream bytestream_pid;
            bytestream_pid = new ByteArrayOutputStream(kPidSize);

            ByteArrayOutputStream bytestream_delay;
            bytestream_delay = new ByteArrayOutputStream(kPidSize);

            DataOutputStream outstream_pid = new DataOutputStream(bytestream_pid);
            outstream_pid.writeInt(pid);

            DataOutputStream outstream_delay = new DataOutputStream(bytestream_delay);
            outstream_delay.writeInt(delay_ms);

            byte pid_buf[] = bytestream_pid.toByteArray();
            byte[] pid_flip_buf = new byte[kPidSize];
            Convertor.ReverseByteArray(pid_buf, pid_flip_buf, 0, kPidSize);

            byte delay_buf[] = bytestream_delay.toByteArray();
            byte[] delay_flip_buf = new byte[kPidSize];
            Convertor.ReverseByteArray(delay_buf, delay_flip_buf, 0, kPidSize);

            byte buf[] = new byte[kHandleSize + kPidSize * 2 + 1];
            for(int i = 0; i < kHandleSize; i++) {
                buf[i] = handle[i];
            }
            for(int i = 0; i < kPidSize; i++) {
                buf[kHandleSize + i] = pid_flip_buf[i];
            }

            for(int i = 0; i < kPidSize; i++) {
                buf[kHandleSize + kPidSize + i] = delay_flip_buf[i];
            }

            buf[kHandleSize + 2 * kPidSize] = delay_type;

            out.write(buf);
            out.flush();
        }
        public void readFromInputStream(InputStream in) throws IOException, ClassNotFoundException {

            int totalBytes = 0;
            byte[] data = new byte[kHandleSize + kPidSize];
            byte[] buf = new byte[kHandleSize + kPidSize];

            while(totalBytes < kHandleSize + kPidSize) {
                int numBytes = in.read(data);
                for(int i = totalBytes; i < totalBytes + numBytes; i++) {
                    buf[i] = data[i - totalBytes]; 
                }
                totalBytes += numBytes;
            }
            handle = new byte[kHandleSize];
            for(int i = 0; i < kHandleSize; i++) {
                handle[i] = buf[i]; 
            }
            //Convertor.ReverseByteArray(buf, ret.handle, 0, kHandleSize);
            byte[] flip_buf = new byte[kPidSize];
            Convertor.ReverseByteArray(buf, flip_buf, kHandleSize, kPidSize);

            ByteArrayInputStream bytestream;
            bytestream = new ByteArrayInputStream(flip_buf);
            DataInputStream instream_rev = new DataInputStream(bytestream);
            pid = instream_rev.readInt();
        } 
    }
    class SpyProb {
        private static final int kGarbageSize = 8;

        private long garbage;   // 8 bytes


        public void readFromInputStream(InputStream in) throws IOException, ClassNotFoundException {

            int totalBytes = 0;
            byte[] data = new byte[kGarbageSize];
            byte[] buf = new byte[kGarbageSize];

            while(totalBytes < kGarbageSize) {
                int numBytes = in.read(data);
                for(int i = totalBytes; i < totalBytes + numBytes; i++) {
                    buf[i] = data[i - totalBytes]; 
                }
                totalBytes += numBytes;
            }
            //Convertor.ReverseByteArray(buf, ret.handle, 0, kHandleSize);
            byte[] flip_buf = new byte[kGarbageSize];
            Convertor.ReverseByteArray(buf, flip_buf, 0, kGarbageSize);

            ByteArrayInputStream bytestream;
            bytestream = new ByteArrayInputStream(flip_buf);
            DataInputStream instream_rev = new DataInputStream(bytestream);
            garbage = instream_rev.readLong();
            //System.out.println("receive garbage: " + garbage);
        }
    }
    private static class HandlerdReply {
        // java enum size is not as integer
        private static final int ALIVE = 0;
        private static final int DEAD = 1;
        private static final int UNCONFIRMED = 2;    
        private static final int UNKNOWN = 3;
        private static final int LAST_STATE = 4;

        static final int kStateSize = 4;

        private Integer state = ALIVE;
        private Boolean hang = false;

        public void setHang() {
            synchronized(hang) {
                hang = true;
            }
        }

        public void setFail() {
            synchronized(state) {
                state = DEAD;
            }
        }

        public void writeToOutStream(OutputStream out) throws IOException {

            boolean shouldHang;
            synchronized (hang) {
                shouldHang = hang;
            }
            if (shouldHang) {
                int x = 2;
                while (shouldHang)
                   x++; 
                x--;
                synchronized (state) {
                    state = DEAD;
                }
            }
            ByteArrayOutputStream bytestream;
            bytestream = new ByteArrayOutputStream(kStateSize);

            DataOutputStream outstream_send = new DataOutputStream(bytestream);
            synchronized(state) {
                outstream_send.writeInt(state);
            }

            byte state_buf[] = bytestream.toByteArray();
            byte[] flip_buf = new byte[kStateSize];
            Convertor.ReverseByteArray(state_buf, flip_buf, 0, kStateSize);

            out.write(flip_buf);
            out.flush();
            //System.out.println("state: " + state);
            synchronized (state) {
                if (state == DEAD) {
                    System.exit(-1);
                }
            }
        }
    }
    
    private static class Convertor {
        public static void ReverseByteArray(byte[] ori, byte[] flip, int str, int len) {
            for(int i = 0; i < len; i++) {
                flip[i] = ori[str + len - 1 - i];
            }
        }
    }

    public static void main(String args[]) {
        FalconSpy spy = new FalconSpy();        
        spy.start();
    }
}
