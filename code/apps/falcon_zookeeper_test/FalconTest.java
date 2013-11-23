

public class FalconTest {
    public static void main(String[] args) {
        FalconSpy spy = new FalconSpy();
        spy.start();
        Saboteur saboteur = new Saboteur();
        saboteur.start();
        FalconFailureDetector fd = new FalconFailureDetector();
        fd.registerMonitored("2", 0, 0);
        fd.registerMonitored("3", 0, 0);
        while(true) {
            try {
                Thread.sleep(1000);
            } 
            catch(InterruptedException e) {
            }
            if(fd.isFailed("2", 0)) {
                System.out.println("server 2 is failed");
		fd.releaseMonitored("2");
		fd.registerMonitored("2", 0, 0);
            }
            if(fd.isFailed("3", 0)) {
                System.out.println("server 3 is failed");
		fd.releaseMonitored("3");
		fd.registerMonitored("3", 0, 0);
            }
        }
    }
}
