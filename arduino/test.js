const SerialPort = require('serialport');

const PORT = '/dev/ttyACM0'
const port = new SerialPort(PORT, {baudRate: 9600});

const URL = 'github.com/araobp/pic16f1-mcu';

port.on('open', line => {
  port.write(URL + '\n');
});

