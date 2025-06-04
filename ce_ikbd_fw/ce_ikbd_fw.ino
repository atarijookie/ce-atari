// Sketch for ATmega808 in tqfp32 package.
// Will serve as ikbd data handler.
// Build for ATmega808 possible using MegaCoreX: https://mcudude.github.io/MegaCoreX/package_MCUdude_MegaCoreX_index.json
// Chip programable via UDPI interface.

#include <EEPROM.h>

// tags to distinguish keyboard data from ST commands
#define UARTMARK_STCMD      0xAA
#define UARTMARK_KEYBDATA   0xBB
#define UARTMARK_ALIVE      0xEE

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
}

void loop() {
  uint8_t data;
  uint32_t lastAlive = 0xffff0000;

  while(true) {
    uint32_t now = millis();

    if((now - lastAlive) >= 1000) { // 1 second passed since last alive sing sent?
      lastAlive = now;
      Serial.write(UARTMARK_ALIVE); // send this mark twice - 1st will be detected as tag, the 2nd will be used as value and just read to be ignored
      Serial.write(UARTMARK_ALIVE);
    }

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
