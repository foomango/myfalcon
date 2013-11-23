
import java.io.*;
import java.net.*;



public class Saboteur extends Thread {
    public static final int kFailBufferSize = 512;
    public void run() {
        ServerSocket saboteurSocket; 
        try {
            saboteurSocket = new ServerSocket();
            saboteurSocket.setReuseAddress(true);
            saboteurSocket.bind(new InetSocketAddress(12345), 10);

            Socket acceptSocket = saboteurSocket.accept();
            byte[] buf = new byte[kFailBufferSize];

            InputStream in = acceptSocket.getInputStream();
            OutputStream out = acceptSocket.getOutputStream();
            in.read(buf, 0, kFailBufferSize); 
            
            String errStr = new String(buf, "US-ASCII").trim();
            if(errStr.equals("lockspy")) {
                //do nothing
            }
            else if(errStr.equals("segfault")) {
                Runtime.getRuntime().halt(1);
            }
            else if(errStr.equals("badloop")) {
                //do something
            }

        }
        catch(IOException e) {
        }

    }
}
