public class Infinite {

    public static void main(String[] args) throws InterruptedException {

        while (true) {
            Thread.sleep(1000);
            System.out.println("Sleeping");
        }
    }
}
