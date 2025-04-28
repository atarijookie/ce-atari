// Sketch for ATmega808 in tqfp32 package.
// Will serve as ikbd data handler.
// Build for ATmega808 possible using MegaCoreX: https://mcudude.github.io/MegaCoreX/package_MCUdude_MegaCoreX_index.json
// Chip programable via UDPI interface.

#include <EEPROM.h>

// tags to distinguish keyboard data from ST commands
#define UARTMARK_STCMD      0xAA
#define UARTMARK_KEYBDATA   0xBB

// pin used for cmd / data separation
#define PIN_CMD_DATA        PIN_PA4

void setup() {
  Serial.begin(19200);    // TX and RX are connected to ESP32         -- PORTMUX - default setup of UART0 is on PA[3:0]
  Serial1.begin(7812);    // TX - data from host, RX - data from IKBD -- PORTMUX - default setup of UART1 is on PC[3:0]

  //------------------------
  // try to remap serial2 before begin as was recommended
  PORTMUX.USARTROUTEA |= PORTMUX_USART2_ALT1_gc;

  Serial2.begin(7812);    // RX - data from ST keyb

  // PORTMUX - DEFAULT setup of UART2 is on PF[3:0] (conflict with oscillator)
  //         - ALT1    setup of UART2 is on PF[6:4]
  // This probably has to be done after the Serial2.begin(), because UartClass::begin() sets this too, 
  // but I didn't find a proper way to define the remap using lib.
  PORTMUX.USARTROUTEA |= PORTMUX_USART2_ALT1_gc;

  pinMode(PIN_CMD_DATA, INPUT_PULLUP);
}

void uartResend(void)
{
  uint8_t data;

  while(digitalRead(PIN_CMD_DATA) == LOW) {
    if(Serial.available() > 0) {    // got data from ESP32?
      data = Serial.read();
      Serial1.write(data);
    }

    if(Serial1.available() > 0) {    // got data from ikbd?
      data = Serial1.read();

      Serial.write(UARTMARK_STCMD);
      Serial.write(data);
    }

    if(Serial2.available() > 0) {    // got data from keyboard?
      data = Serial2.read();

      Serial.write(UARTMARK_KEYBDATA);
      Serial.write(data);
    }
  }
}

void eepromStoreFromBuffer(int eepromStartAddress, uint8_t* bfr, uint8_t maxSize)
{
  int i;
  for(i=0; i<maxSize; i++) {    // go through the buffer, up to max size
    EEPROM.write(eepromStartAddress + i, bfr[i]);   // write value to eeprom

    if(bfr[i] == 0) {   // zero terminated end of string found, quit
      return;
    }
  }

  // if we got here, we've reached the end of input buffer, so terminate string at the end
  EEPROM.write(eepromStartAddress + maxSize - 1, 0);
}

void eepromReadToSerial(int eepromStartAddress, uint8_t maxSize)
{
  int i;

  for(i=0; i<maxSize; i++) {    // go through the buffer, up to max size
    uint8_t data = EEPROM.read(eepromStartAddress + i);
    Serial.write(data);

    if(data == 0) {   // zero terminated end of string found, quit
      break;
    }
  }

  Serial.write('\n');
}

void handleEsp32Commands(void)
{
  #define BUFFER_SIZE 40
  uint8_t buffer[BUFFER_SIZE];
  uint8_t cnt = 0;

  #define EEPROM_ADDR_SSID      0
  #define EEPROM_ADDR_PSWD      BUFFER_SIZE

  while(digitalRead(PIN_CMD_DATA) == HIGH) {
    if(Serial.available() <=0) {    // no data? just wait for more data or controll pin change
      continue;
    }

    uint8_t data = Serial.read();

    if(cnt < (BUFFER_SIZE-2)) {   // buffer not full? add to buffer
      buffer[cnt] = data;
      cnt++;
    }

    if(data == '\n') {      // EOL? handle data
      buffer[cnt - 1] = 0;  // zero terminate string by replacing last '\n' with zero

      if(buffer[0] == 'W') {        // write?
        switch(buffer[1]) {
          case 'S': eepromStoreFromBuffer(EEPROM_ADDR_SSID, buffer + 2, BUFFER_SIZE - 2); break;    // write SSID
          case 'P': eepromStoreFromBuffer(EEPROM_ADDR_PSWD, buffer + 2, BUFFER_SIZE - 2); break;    // write password
        }
      } else if(buffer[0] == 'R') {   // read?
        switch(buffer[1]) {
          case 'S': eepromReadToSerial(EEPROM_ADDR_SSID, BUFFER_SIZE - 2); break;    // read SSID
          case 'P': eepromReadToSerial(EEPROM_ADDR_PSWD, BUFFER_SIZE - 2); break;    // read password
        }
      }

      cnt = 0;              // restart receiving at the buffer start
    }
  }
}

void loop() {
  while(true) {
    if(digitalRead(PIN_CMD_DATA) == HIGH) {   // if high, handle commands from ESP32
      handleEsp32Commands();
    } else {    // if low, just pass data through UARTs
      uartResend();
    }
  }
}
