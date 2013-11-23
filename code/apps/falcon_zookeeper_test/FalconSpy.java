
import java.lang.management.ManagementFactory;
import java.lang.Long;
import java.io.*;

import com.google.code.juds.*; 


public class FalconSpy extends Thread {
    private static final String socketFile = "/dev/ntfa";

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
            HandlerdReply reply = new HandlerdReply();
            for(;;) {
                prob.readFromInputStream(in);
                reply.writeToOutStream(out);
                System.out.println("send alive to handlerd\n");
            }
        }
        catch (Exception e) {
            System.out.println("Exception in FalconSpy");
            System.out.println(e);
            if(socket != null) {
                socket.close();
            }
        }
    }
    private static class HandlerdHandshake {
        private static final int kHandleSize = 32;
        private static final int kPidSize = 4;
        private byte[] handle;  //32bytes
        private int pid;       //32bits, 4bytes

        public HandlerdHandshake(String s) {
            //Long lpid = new Long(Thread.currentThread().getId());
            String strs[] =
                ManagementFactory.getRuntimeMXBean().getName().split("@");
            //System.out.println("lpid: " + lpid);
            //pid = lpid.intValue();
            pid = Integer.parseInt(strs[0]);
            System.out.println("pid: " + pid);
            handle = new byte[kHandleSize];

            try {
                System.arraycopy(s.getBytes("US-ASCII"), 0, handle, 0, s.length());
            }
            catch (UnsupportedEncodingException e) {
            }
        }
        public void writeToOutStream(OutputStream out) throws IOException {
            ByteArrayOutputStream bytestream;
            bytestream = new ByteArrayOutputStream(kPidSize);

            DataOutputStream outstream_send = new DataOutputStream(bytestream);
            outstream_send.writeInt(pid);

            byte pid_buf[] = bytestream.toByteArray();
            byte[] flip_buf = new byte[kPidSize];
            Convertor.ReverseByteArray(pid_buf, flip_buf, 0, kPidSize);

            byte buf[] = new byte[kHandleSize + kPidSize];
            for(int i = 0; i < kHandleSize; i++) {
                buf[i] = handle[i];
            }
            for(int i = 0; i < kPidSize; i++) {
                buf[kHandleSize + i] = flip_buf[i];
            }
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
            System.out.println("receive garbage: " + garbage);
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

        private int state;

        public HandlerdReply() {
            state = ALIVE;
        }

        public void writeToOutStream(OutputStream out) throws IOException {

            ByteArrayOutputStream bytestream;
            bytestream = new ByteArrayOutputStream(kStateSize);

            DataOutputStream outstream_send = new DataOutputStream(bytestream);
            outstream_send.writeInt(state);

            byte state_buf[] = bytestream.toByteArray();
            byte[] flip_buf = new byte[kStateSize];
            Convertor.ReverseByteArray(state_buf, flip_buf, 0, kStateSize);

            out.write(flip_buf);
            out.flush();
            System.out.println("state: " + state);
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
