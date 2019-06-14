 #include <Wire.h>

// SO8 package of ST25DV04K
#define SO8

// LEDs
int LED_GREEN = 5;
int LED_BLUE = 4;
int LED_RED = 2;

//#define CAP_SEN

#ifdef CAP_SEN
// Capacitance sensor
int CAP_SEN0 = 8;
#endif

// GPO (General Purpose Output) of ST25DV04K
int GPO = 12;

// Device select (7bit I2C addresses)
int E2_0 = 0x53;
int E2_1 = 0x57;

// RF management
uint8_t NORMAL = 0x00;
uint8_t DISABLE = 0x01;  // Error returned
uint8_t SLEEP = 0x02;    // Stay silent

// Input buffer for serial
char input_buf[256];

// State machine
typedef enum {
  PHASE0, PHASE1, PHASE2
} tag_state;

tag_state state;

// Variables for setup() and loop()
int idx = 0;
bool hasInput = false;
unsigned long last_gpo_event;
unsigned long last_touched;
char c;

/*---- Basic functions ---------------------------*/

// Dump data
void dump(uint8_t dev_addr, uint8_t reg_addr_msb, uint8_t reg_addr_lsb, uint8_t len, bool hex) {
  Wire.beginTransmission(dev_addr);
  Wire.write(reg_addr_msb);
  Wire.write(reg_addr_lsb);
  Wire.endTransmission(false);
  Wire.requestFrom((int)dev_addr, (int)len, true);
  for (int i=0; i < len; i++) {
    if (hex) {
      int d = Wire.read();
      Serial.print(d, HEX);
      Serial.print(' ');
    } else {
      char c = Wire.read();
      Serial.print(c);
    }
  }
  Serial.println();  
}

// Read one byte and return it
uint8_t read_one_byte(uint8_t dev_addr, uint8_t reg_addr_msb, uint8_t reg_addr_lsb) {
  Wire.beginTransmission(dev_addr);
  Wire.write(reg_addr_msb);
  Wire.write(reg_addr_lsb);
  Wire.endTransmission(false);
  Wire.requestFrom((int)dev_addr, 0x01, true);
  return Wire.read();
}

// Write data
void write(uint8_t dev_addr, uint8_t reg_addr_msb, uint8_t reg_addr_lsb, uint8_t *data, uint8_t len) {
  Wire.beginTransmission(dev_addr);
  Wire.write(reg_addr_msb);
  Wire.write(reg_addr_lsb);
  for (int i=0; i < len; i++) {
    Wire.write(data[i]);
  }
  Wire.endTransmission(true);
  delay(100);  // TODO: polling
}

/*---- Utility functions ---------------------------*/

void dump_system_config(void) {
  dump(E2_1, 0x00, 0x00, 0x20, true);
}

void present_password(void) {
  // Default password is {0, 0, 0, 0, 0, 0, 0, 0}
  uint8_t data[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0x09, 0, 0, 0, 0, 0, 0, 0, 0};
  write(E2_1, 0x09, 0x00, data, 17);
}

void dump_password(void) {
  dump(E2_1, 0x09, 0x00, 8, true);
}

void dump_dynamic_config(void) {
  dump(E2_0, 0x20, 0x00, 8, true);
}

void dump_CC_file(void) {
  // In case of ST25DV04K, it should be "E1 40 40 00h" (Application Note: AN4911)
  dump(E2_0, 0x00, 0x00, 0x04, true);
}

int dump_NDEF_message_type(void) {
  dump(E2_0, 0x00, 0x04, 1, true); 
}

int dump_NDEF_message_length(void) {
  dump(E2_0, 0x00, 0x05, 1, true);   
}

int dump_NDEF_header(void) {
  dump(E2_0, 0x00, 0x06, 4, true); 
}

void dump_NDEF_payload_length(void) {
  dump(E2_0, 0x00, 0x09, 1, true);   
}

void dump_NDEF_payload_identifier_code(void) {
  dump(E2_0, 0x00, 0x0a, 1, true);   
}

void dump_NDEF_payload_uri_field(void) {
  uint8_t len = read_one_byte(E2_0, 0x00, 0x08) - 1;
  int repeat = len / 32;  // Wire lib's buffer size
  for (int i=0; i < repeat; i++) {
    dump(E2_0, 0x00, 0x0b+i*32, 32, false);
  }
  dump(E2_0, 0x00, 0x0b+repeat*32, len-32*repeat, false);    
}

// Area 0 only, read allowed, write prohibited
void reset_areas(void) {
  uint8_t ENDA1 = 0x0F;
  uint8_t ENDA2 = 0x0F;
  uint8_t ENDA3 = 0x0F;
  uint8_t READ_ONLY = 0x0C;
  write(E2_1, 0x00, 0x0a, &READ_ONLY, 1);  
  write(E2_1, 0x00, 0x09, &ENDA3, 1);  // ENDA3
  write(E2_1, 0x00, 0x08, &READ_ONLY, 1);  
  write(E2_1, 0x00, 0x07, &ENDA2, 1);  // ENDA2
  write(E2_1, 0x00, 0x06, &READ_ONLY, 1);  
  write(E2_1, 0x00, 0x05, &ENDA1, 1);  // ENDA1
  write(E2_1, 0x00, 0x04, &READ_ONLY, 1);  
}

// RF Management
void manage_rf(uint8_t state) {
  write(E2_1, 0x00, 0x03, &state, 1);
}

// Write HTTPS URL
void write_URL(char *pUrl) {
  uint8_t CC_HEADER[] = { 0xE1, 0x40, 0x40, 0x00, 0x03, 0x00, 0xD1, 0x01, 0x00, 0x55 };
  uint8_t HTTPS = 0x04;
  uint8_t len = strlen(pUrl) + 1; // Including "HTTPS"
  uint8_t total_len = len + 10;
  uint8_t buf[256];

  CC_HEADER[5] = len + 4;
  CC_HEADER[8] = len;

  memcpy(buf, CC_HEADER, 10);
  buf[10] = HTTPS;
  memcpy(buf+11, pUrl, len-1);
  
  // TODO: Why 30 for write??? 32 did not work.
  int repeat = total_len / 30;  // Wire lib's buffer size.
  for (int i=0; i < repeat; i++) {
    write(E2_0, 0x00, 0x00+i*30, buf+i*30, 30);      
  }
  write(E2_0, 0x00, 0x00+repeat*30, buf+repeat*30, total_len-30*repeat);
}

// This is just to know that Arduino is restarting by watching LEDs
void led_startup(void) {
  digitalWrite( LED_GREEN, HIGH );
  delay(300);
  digitalWrite( LED_BLUE, HIGH );
  delay(300);
  digitalWrite( LED_RED, HIGH );
  delay(300);
  digitalWrite( LED_GREEN, LOW );
  delay(300);
  digitalWrite( LED_BLUE, LOW );
  delay(300);
  digitalWrite( LED_RED, LOW );
  delay(300);  
}

// This is for a debug purpose
void startup_message(void) {
  Serial.print("ST25 system config: ");
  dump_system_config();
  Serial.print("ST25 password: ");
  dump_password();
  Serial.print("ST25 dynamic config: ");
  dump_dynamic_config();
  Serial.print("NDEF CC file: ");
  dump_CC_file();
  Serial.print("NDEF message type: ");
  dump_NDEF_message_type();
  Serial.print("NDEF message length: ");
  dump_NDEF_message_length();
  Serial.print("NDEF header: ");
  dump_NDEF_header();
  Serial.print("NDEF payload length: ");
  dump_NDEF_payload_length();  
  Serial.print("NDEF Identifier code: ");
  dump_NDEF_payload_identifier_code();
  Serial.println("NDEF URI field:");
  dump_NDEF_payload_uri_field();  
}

bool timer_expired(unsigned long last_time, unsigned long timer) {
  
  unsigned long current_time = millis();
  bool expired = false;
  
  if (current_time >= last_time) {
    if ((current_time - last_time) > timer) {
      expired = true;
    }
  } else {  // Rollover (millis() max value is 0xffffffff)
    if ((0xffffffff - last_time + current_time) > timer) {
      expired = true;
    }
  }
  return expired;
}  

/*------ Main routine ---------------------------*/

void setup() {
  
  pinMode(LED_GREEN, OUTPUT);  // GREEN
  pinMode(LED_BLUE, OUTPUT);  // BLUE
  pinMode(LED_RED, OUTPUT);  // RED
#ifdef CAP_SEN
  pinMode(CAP_SEN0, INPUT);  // Capacitance sensor
#endif
  pinMode(GPO, INPUT);  // GPO

  Wire.begin();  // I2C bus
  Wire.setClock(400000);  // SCK: 400kHz

  // Present I2C password
  // TODO: finish I2C secure context
  present_password();

  //reset_areas();
  //char url[] = "github.com/araobp/pic16f1-mcu/blob/master/BLINKERS.md";
  //write_URL(url);
  
  led_startup();
  manage_rf(DISABLE);
  state = PHASE0;

  last_touched = millis();
  
  // Begin serial communcation 
  Serial.begin( 9600 );

}

/*
 * <<< Main routine: state machine >>>
 * 
 * [1] New URL reception from a host
 * 
 * New URL is not written onto EEPROM but rather kept 
 * in a variable at this timing to avoid wearing.
 * 
 * [2] RF state management
 * 
 * DISABLE ---> SLEEP ---> NORMAL ---> DISABLE
 *       PHASE0      PHASE1     PHASE2
 *     GPO event   Write URI   RF in Service
 *                 on demand
 */
void loop() {
  
  int timer;

  // URL write command reception
  while (Serial.available() > 0) {
    c = Serial.read();    
    if (c == '\n' || idx >= 255) {
      input_buf[idx] = '\0';
      hasInput = true; 
    } else {
      input_buf[idx++] = c;
    }
  }

  if (hasInput) {
    if (input_buf[0] == '.') {
      switch (input_buf[1]) {
        case '0':
          Serial.println("LED0");
          break;
        case '1':
          Serial.println("LED1");
          break;
        case '2':
          Serial.println("LED2");
          break;
        case '3':
          Serial.println("LED3");
          break;
        case '4':
          Serial.println("LED4");
          break;
        default:
          break;
      }
    } else {
      write_URL(input_buf);
    }
    idx = 0;
    hasInput = false;
  }
  
#ifdef CAP_SEN
  if (digitalRead(CAP_SEN0) == HIGH && timer_expired(last_touched, 1000UL)) {  // 1sec
    Serial.println("CAP_SEN_TOUCHED");
    last_touched = millis();
  }
#endif

  switch(state) {

    case PHASE0: // monitor pin state

      // GPO
    #ifdef SO8
      if (digitalRead(GPO) == LOW) {  // Open drain
    #else
      if (digitalRead(GPO) == HIGH) {  // CMOS
    #endif
        manage_rf(SLEEP);
        Serial.println("RF_FIELD_CHANGE");
        last_gpo_event = millis();
        state = PHASE1;
        Serial.println("PHASE1");
      }
      break;

    case PHASE1:  // Write a new URL
      write_URL(input_buf);
      manage_rf(NORMAL);
      state = PHASE2;
      Serial.println("PHASE2");
      break;
      
    case PHASE2:  // in service
      if (timer_expired(last_gpo_event, 5000UL)) {  // 5sec
        manage_rf(DISABLE);
        state = PHASE0;
        Serial.println("PHASE0");      
      }
      break;
  }
  
}
