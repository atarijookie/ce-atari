#include "WiFi.h"
#include <Preferences.h>

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "ikbd.h"

extern Preferences preferences;
extern NetworkClient clientIkbd;

extern String hostIpString;
extern uint16_t hostPortIkbd;
extern bool connected;          // if true, wifi is connected

volatile bool ikbdEnabled = true;   // if true, should send data to host; otherwise just loopback ikdb data back
volatile bool ikbdAlive = false;    // if true, data is comming from ikdb

TaskHandle_t xIkbdTask;

void onIkdbDisabled(int availableSerial)
{
    // data comes in as pairs (mark + value), so read it by pairs, when there is at least a pair of data available
    while(availableSerial >= 2)
    {
        uint8_t mark = Serial1.read();
        uint8_t value = Serial1.read();
        availableSerial -= 2;

        if(mark != UARTMARK_KEYBDATA) {     // ignore anything that's not the data from keyboard
            continue;
        }

        Serial1.write(value);               // write keyboard data back to serial
    }
}

void onIkbdEnabled(int availableSerial)
{
    #define IKBD_BFR_SIZE 128
    uint8_t buffer[IKBD_BFR_SIZE];
    int readSize;

    // got at least 2 bytes? read from serial, send to socket
    if(availableSerial >= 2)
    {
        readSize = MIN(availableSerial, IKBD_BFR_SIZE);
        Serial1.read(buffer, readSize);
        clientIkbd.write(buffer, readSize);
    }

    // got something available from socket? read from socket, send to serial
    int availableSock = clientIkbd.available();
    if(availableSock > 0)
    {
        int readSize = MIN(availableSock, IKBD_BFR_SIZE);
        readSize = clientIkbd.read(buffer, readSize);
        Serial1.write(buffer, readSize);
    }
}

void taskIkbd(void* pvParameters)
{
    uint32_t lastReceivedTime = 0xffff0000;     // when was some data last received from ikdb
    Serial.println("I starting");

    uint32_t lastStatus = 0;

    while(true)
    {
        vTaskDelay(20);          // intentionally process only once a while

        uint32_t now = millis();
        int availableSerial = Serial1.available();

        if(availableSerial > 0)       // data available? mark current time as time when data was received
        {
            lastReceivedTime = now;
        }

        if((now - lastStatus) >= 1000)
        {
            lastStatus = now;
/*
            Serial.print("I alive ");
            Serial.print(ikbdAlive);
            Serial.print(" enabled ");
            Serial.print(ikbdEnabled);
            Serial.print(" connected ");
            Serial.print(clientIkbd.connected());
            Serial.print(" available ");
            Serial.println(availableSerial);
*/
        }

        ikbdAlive = (now - lastReceivedTime) < 5000;  // ikdb is alive when received data recently

        // ikbd is enabled, ikdb chip is sending data (chip present), wifi is connected, but our ikbd socket is NOT connected, connect now
        if(ikbdEnabled && ikbdAlive && connected && !clientIkbd.connected())
        {
            Serial.println("I connect");
            clientIkbd.connect(hostIpString.c_str(), hostPortIkbd);
            clientIkbd.setNoDelay(true);
        }

        // ikbd not sending data (chip not present) or ikbd not enabled, but the ikbd socket is connected, then disconnect
        if((!ikbdAlive || !ikbdEnabled) && clientIkbd.connected())
        {
            Serial.println("I disconnect");
            clientIkbd.stop();
        }

        // check what is the 1st byte in the received data, if it's not a know mark, just read it and ignore it
        uint8_t firstByte = Serial1.peek();

        if(firstByte != UARTMARK_STCMD && firstByte != UARTMARK_KEYBDATA && firstByte != UARTMARK_ALIVE) {
            Serial1.read();
            availableSerial--;
        }

        availableSerial = availableSerial & 0xfffffffe;     // remove lowest bit, turning available into even number

        // ikbd enabled and ikbd socket is connected? send and get data to/from host
        if(ikbdEnabled && clientIkbd.connected())
        {
            onIkbdEnabled(availableSerial);
        } 
        else    // ikbd disabled or just socket not connected to host? send data directly to Atari
        {
            onIkdbDisabled(availableSerial);
        }
    }
}

void createIkbdTask(void)
{
    BaseType_t xReturned;
    xReturned = xTaskCreatePinnedToCore(taskIkbd, "taskIkbd", 8192, (void *) NULL, /*priority*/ 2, &xIkbdTask, 0);
}
