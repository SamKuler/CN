using System;
using System.Text;
using System.Net;
using System.Net.Sockets;

namespace UDPClient
{
    class Program
    {
        static void Main(string[] args)
        {
            var theSock = new UdpClient("127.0.0.1", 9876);
            for (int i = 0; i <= 50; i++)
            {
                var sendStr = i.ToString();
                var myData = Encoding.ASCII.GetBytes(sendStr);
                theSock.Send(myData, myData.Length);
                var server = new IPEndPoint(IPAddress.Any, 0);
                var data = theSock.Receive(ref server);
                var recvStr = Encoding.ASCII.GetString(data);
                Console.WriteLine("Received from server: " + recvStr);
            }
            theSock.Close();
        }
    }
}
