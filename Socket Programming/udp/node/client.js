var PORT = 9876;
var HOST = '127.0.0.1';

var dgram = require('dgram');
var client = dgram.createSocket('udp4');

let inactivityTimeout;

function closeClientAfterInactivity() {
  console.log('Client shutdown for inactivity.');
  client.close();
}

client.on('message', function (message) {
  console.log(`Received from server: ${message.toString()}`);

  // Reset the timeout
  clearTimeout(inactivityTimeout);
  inactivityTimeout = setTimeout(closeClientAfterInactivity, 500);
});

for (let i = 0; i <= 50; i++) {
  const buffer = Buffer.from(i.toString());
  client.send(buffer, 0, buffer.length, PORT, HOST);
}

inactivityTimeout = setTimeout(closeClientAfterInactivity, 1000);