# Arduino with dynamic NFC tag

<img src="./doc/shield.jpg" width=400>

## Parts

- RasPi 3
- Arduino UNO
- [ANT7-T-ST25DV04K (dynamic NFC tag)](https://www.st.com/en/evaluation-tools/ant7-t-st25dv04k.html)

## Schematic of Dynamic NFC tag Arduino shield

This is a schematic of Arduino shield with ANT7-T-ST25DV04K. Its pin assignment is same as that of STMicro's expansion board "X-NUCLEO-NFC04A1".

==> [schematic](./kicad)

Note: the shield is powered by 3V3 pin on Arduino UNO. Although Arduino UNO is a 5V system, the circuit works.

### Attaching the nfc tag module to the shield

The module's RF pin (GPO) plays an important role in this project to avoid EEPROM wearing. Arduino can sense that a smart phone is in its proximity by checking voltage of GPO.

I have come up with the following method: connect a pin socket to RF pin of the module with a wire.

<img src="./doc/ant7-t-st25dv04k_1.jpg" width=200>

<img src="./doc/ant7-t-st25dv04k_2.jpg" width=200>

## Sample app: "Digital photo frame with dynamic NFC tag"

Architecture

```
   [Chrome]----socket----[webapp.js]----USB----[Arduino]
   
```

==> [webapp.js](./webapp)

[Step 0] Connect the device (Arduino with the shield) to RasPi with an USB cable.

[Step 1] Start webapp.js

```
$ node webapp.js
```

[Step 2] Access "http://localhost:18080" with Chrome browser on RasPi to start the digial photo frame.

[Step 3] Hold a NFC-capable smart phone over the NFC tag. It should start a browser automatically to show a Wikipedia URL with the image.

## Explanation on NDEF

### NDEF records on ST25DV04K' EEPROM

The following is an example of a record in Area 1 on the EEPROM embedded in ST25DV04K:

```
ST25 system config: 88 3 1 0 C 7 0 E C F 0 0 0 0 7 0 0 0 0 0 7F 0 3 24 AF F 62 2 0 24 2 E0 
ST25 password: 0 0 0 0 0 0 0 0 
ST25 dynamic config: 88 FF 8 0 1 0 0 0 
NDEF CC file: E1 40 40 0 
NDEF message type: 3 
NDEF message length: 3A 
NDEF header: D1 1 36 55 
NDEF payload length: 55 
NDEF Identifier code: 4 
NDEF URI field:
github.com/araobp/pic16f1-mcu/bl
ob/master/BLINKERS.md
```

### NDEF format for URI(HTTPS)

```
+---------------+
|     0xE1      | CC File 4 bytes length
+---------------+ 
|     0x40      | CC File
+---------------+
|     0x40      | CC File
+---------------+
|     0x00      | CC File
+---------------+
|     0x03      | Meaning that this tag contains NDEF records.
+---------------+
|Payload len + 4| NDEF Head 4 bytes (short record, no ID)
+---------------+
|1|1|0|1|0|0|0|1| MB(1), ME(1), CF(0), SR(1), IL(0), TNF(001)
+---------------+
|     0x01      | Type length
+---------------+
|Payload length |
+---------------+
|    0x55 ('U') | Type: URI
+---------------+
|     0x04      | Identifier code: HTTPS
+---------------+
|     URL       | Payload
|      :        |
|      :        |
+---------------+
```
