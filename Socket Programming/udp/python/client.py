import socket
 
size = 8192
 
try:
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  for i in range(0,51):
    msg = str(i).encode('utf-8')
    sock.sendto(msg, ('localhost', 9876))
    recv_msg = sock.recv(size)
    print(f"Received from server: {recv_msg.decode('utf-8')}")
  sock.close()
  
except Exception as e:
  print("cannot reach the server: ", e)
