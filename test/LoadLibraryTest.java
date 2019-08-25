import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;

class LoadLibraryTest {

    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 200; i++) {
            Thread.sleep(10);
        }

        // Late load of libnet.so and libnio.so
        DatagramChannel ch = DatagramChannel.open();
        ch.bind(new InetSocketAddress(0));

        ByteBuffer buf = ByteBuffer.allocateDirect(1000);
        InetSocketAddress target = new InetSocketAddress(InetAddress.getLoopbackAddress(), 1024);

        while (true) {
            ch.send(buf, target);
            buf.clear();
        }
    }
}
