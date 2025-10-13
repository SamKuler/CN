import java.net.*;

class UDPServer {
    static int count = 0;

    public static void main(String args[]) throws Exception {
        try (DatagramSocket serverSocket = new DatagramSocket(9876)) {
            while (true) {
                byte[] receiveData = new byte[1024];
                DatagramPacket receivePacket = new DatagramPacket(receiveData, receiveData.length);
                serverSocket.receive(receivePacket);
                String sentence = new String(receivePacket.getData(), 0, receivePacket.getLength());
                InetAddress IPAddress = receivePacket.getAddress();
                int port = receivePacket.getPort();
                count++;
                String sendString = Integer.toString(count) + " " + sentence;
                byte[] sendData = sendString.getBytes();
                DatagramPacket sendPacket = new DatagramPacket(sendData, sendData.length, IPAddress, port);
                serverSocket.send(sendPacket);
            }
        }
    }
}