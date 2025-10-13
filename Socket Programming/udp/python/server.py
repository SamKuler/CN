import socket

size = 8192

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', 9876))

count = 0

try:
  while True:
    data, address = sock.recvfrom(size)
    count += 1
    new_data = f"{count} {data.decode('utf-8')}".encode('utf-8')
    sock.sendto(new_data, address)
finally:
  sock.close()