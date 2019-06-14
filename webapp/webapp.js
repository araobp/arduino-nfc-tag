/*
 * Digital photo frame with dynamic NFC tag
 */

// Interface to dynamic NFC tag (Arduino with ST25DV04K)
const SerialPort = require('serialport');
const Readline = require('@serialport/parser-readline');
const TAG_PORT = '/dev/ttyACM0';
const port = new SerialPort(TAG_PORT, {baudRate: 9600});
const parser = new Readline();
port.pipe(parser);
parser.on('data', data => console.log(data));

// URLs to be written onto the NFC tag via I2C
const URLs = ['en.wikipedia.org/wiki/Namdaemun',
  'en.wikipedia.org/wiki/Seoul_station',
  'en.wikipedia.org/wiki/Gangneung',
  'en.wikipedia.org/wiki/Gyeongbokgung',
  'en.wikipedia.org/wiki/Dohwa-dong,_Seoul'
    ];

// Web app server based on express
const express = require('express');
const app = express();
const PORT = 18080;

app.use(express.static(__dirname));

app.get('/images', (req, res) => {
  let num = req.query.num;
  console.log('GET request for ' + num);
  res.sendFile(__dirname + '/images/' + num + '.jpg');
});

app.post('/images', (req, res) => {
  let num = req.query.num;
  console.log('POST request for ' + num);
  port.write(URLs[num] + '\n');
  res.send('OK');
});

const server = app.listen(PORT, () => {
  console.log('server started');
});

