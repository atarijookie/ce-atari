#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "ikbd.h"

// extern WiFiClient clientIkbd;

extern std::string hostIpString;
extern uint16_t hostPortIkbd;
extern bool connected;              // if true, wifi is connected

volatile bool ikbdEnabled = true;   // if true, should send data to host; otherwise just loopback ikdb data back

// TaskHandle_t xIkbdTask;

#define IKBD_BFR_SIZE 128
uint8_t buffer[IKBD_BFR_SIZE];

void onIkdbDisabled(void)
{
    // ikbd not sending data (chip not present) or ikbd not enabled, but the ikbd socket is connected, then disconnect
    // if any data comming from host via socket is available, read and and drop it
/*
    while(clientIkbd.available() > 0)
    {
        int available = clientIkbd.available();
        int readSize = MIN(available, IKBD_BFR_SIZE);
        clientIkbd.read(buffer, readSize);
    }

    while(Serial1.available() > 0)  // got data from KEYB_TX_ORIG? just send it back to KEYB_TX
    {
        uint8_t data = Serial1.read();
        Serial1.write(data);
    }

    while(Serial2.available() > 0) {    // got data from Atari? Just read it and ignore it
        Serial2.read();
    }
    */
}

void onIkbdEnabled(void)
{
    int readSize;
    uint8_t data;

    /* TODO:
    // keep sending forwarding data around until all the sources are empty
    while(Serial1.available() || clientIkbd.available())
    {
        if(Serial1.available() > 0)     // got data from KEYB_TX_ORIG? send it to host with tag
        {
            data = Serial1.read();
            clientIkbd.write(UARTMARK_KEYBDATA);
            clientIkbd.write(data);
        }

        if(Serial2.available() > 0)     // got data from KEYB_RX? send it to host with tag
        {
            data = Serial2.read();
            clientIkbd.write(UARTMARK_STCMD);
            clientIkbd.write(data);
        }

        if(clientIkbd.available() > 0)  // got data from host? send it to Atari
        {
            data = clientIkbd.read();
            Serial1.write(data);
        }
    }
    */
}

void ikbdConnectDisconnect(void)
{
    /*
    if(ikbdEnabled)
    {
        // ikbd is enabled, ikdb chip is sending data (chip present), wifi is connected, but our ikbd socket is NOT connected, connect now
        if(connected && !clientIkbd.connected())
        {
            printf("I connect\n");
            clientIkbd.connect(hostIpString.c_str(), hostPortIkbd);
            clientIkbd.setNoDelay(true);
        }
    }
    else
    {
        // ikbd not sending data (chip not present) or ikbd not enabled, but the ikbd socket is connected, then disconnect
        if(clientIkbd.connected())
        {
            printf("I disconnect\n");
            clientIkbd.stop();
        }
    }
    */
}

void taskIkbd(void* pvParameters)
{
    uint32_t lastReceivedTime = 0xffff0000;     // when was some data last received from ikdb
    printf("I starting\n");

    uint32_t lastStatus = 0;

    while(true)
    {
        // TODO:
        // vTaskDelay(20);          // intentionally process only once a while
/*
        uint32_t now = millis();

        if((now - lastStatus) >= 1000)  // once per second
        {
            lastStatus = now;

            if(clientIkbd.connected())  // if connected, send ALIVE mark and value every second
            {
                clientIkbd.write(UARTMARK_ALIVE);
                clientIkbd.write(UARTMARK_ALIVE);
            }

            // printf("I enabled %d, connected: %d\n", ikbdEnabled, clientIkbd.connected());
        }

        ikbdConnectDisconnect();

        // ikbd enabled and ikbd socket is connected? send and get data to/from host
        if(ikbdEnabled && clientIkbd.connected())
        {
            onIkbdEnabled();
        } 
        else    // ikbd disabled or just socket not connected to host? send data directly to Atari
        {
            onIkdbDisabled();
        }
*/
    }
}

void createIkbdTask(void)
{
    // BaseType_t xReturned;
    // xReturned = xTaskCreatePinnedToCore(taskIkbd, "taskIkbd", 8192, (void *) NULL, /*priority*/ 2, &xIkbdTask, 0);
}
