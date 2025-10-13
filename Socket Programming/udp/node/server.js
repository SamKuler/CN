var PORT = 9876;
var HOST = '0.0.0.0';

var dgram = require('dgram');
var server = dgram.createSocket('udp4');

let count = 0;

server.on('message', function (message, remote) {
  count++;
  const msg = `${count} ${message.toString()}`;
  const buffer = Buffer.from(msg);
  server.send(buffer, 0, buffer.length, remote.port, remote.address);
});

server.bind(PORT, HOST);