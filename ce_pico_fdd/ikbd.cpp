#include "hardware/uart.h"

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "ikbd.h"

extern ip_addr_t hostIpAddr;
extern std::string hostIpString;
extern uint16_t hostPortIkbd;
extern bool connectedToWifi;              // if true, wifi is connected

extern Settings_t Settings;
TConnection connectionIkbd;

#define IKBD_BFR_SIZE 128
uint8_t buffer[IKBD_BFR_SIZE];

void onIkdbDisabled(void)
{
    // ikbd not sending data (chip not present) or ikbd not enabled, but the ikbd socket is connected, then disconnect
    // if any data comming from host via socket is available, read and and drop it
    while(1)
    {
        int available = connectionCanReadBytes(&connectionIkbd);
        if(available <= 0) {    // nothing more to read here? quit this loop
            break;
        }

        int readSize = MIN(available, IKBD_BFR_SIZE);
        conRead(&connectionIkbd, buffer, readSize);
    }

    while(uart_is_readable(uart1))           // got data from KEYB_TX_ORIG? just send it back to KEYB_TX
    {
        uint8_t data = uart_getc(uart1);
        uart_putc(uart1, data);
    }

    while(uart_is_readable(uart0) > 0) {    // got data from Atari? Just read it and ignore it
        uart_getc(uart0);
    }
}

void onIkbdEnabled(void)
{
    uint8_t data[16];

    // keep sending forwarding data around until all the sources are empty
    while(uart_is_readable(uart1) || connectionCanReadBytes(&connectionIkbd))
    {
        if(uart_is_readable(uart1))          // got data from KEYB_TX_ORIG? send it to host with tag
        {
            data[0] = UARTMARK_KEYBDATA;
            data[1] = uart_getc(uart1);
            conWrite(&connectionIkbd, data, 2);
        }

        if(uart_is_readable(uart0) > 0)     // got data from KEYB_RX? send it to host with tag
        {
            data[0] = UARTMARK_STCMD;
            data[1] = uart_getc(uart0);
            conWrite(&connectionIkbd, data, 2);
        }

        int readSize = MIN(connectionCanReadBytes(&connectionIkbd), sizeof(data));
        if(readSize > 0)  // got data from host? send it to Atari
        {
            conRead(&connectionIkbd, data, readSize);
            uart_write_blocking(uart1, data, readSize);
        }
    }
}

void ikbdConnectDisconnect(void)
{
    if(Settings.ikbdEnabled)
    {
        // ikbd is enabled, ikdb chip is sending data (chip present), wifi is connected, but our ikbd socket is NOT connected, connect now
        if(connectedToWifi && !isConnected(&connectionIkbd))
        {
            debug("I connect\n");
            connect(&connectionIkbd, &hostIpAddr, hostPortIkbd);
        }
    }
    else
    {
        // ikbd not sending data (chip not present) or ikbd not enabled, but the ikbd socket is connected, then disconnect
        if(isConnected(&connectionIkbd))
        {
            debug("I disconnect\n");
            stop(&connectionIkbd);
        }
    }
}

void ikbdHandling(void)
{
    static uint32_t lastStatus = 0;

    uint32_t now = millis();

    if((now - lastStatus) >= 1000)  // once per second
    {
        lastStatus = now;

        if(isConnected(&connectionIkbd))  // if connected, send ALIVE mark and value every second
        {
            uint8_t data[2] = {UARTMARK_ALIVE, UARTMARK_ALIVE};
            conWrite(&connectionIkbd, data, 2);
        }

        // debug("I enabled %d, connected: %d\n", Settings.ikbdEnabled, isConnected(&connectionIkbd));
    }

    ikbdConnectDisconnect();

    // ikbd enabled and ikbd socket is connected? send and get data to/from host
    if(Settings.ikbdEnabled && isConnected(&connectionIkbd))
    {
        onIkbdEnabled();
    }
    else    // ikbd disabled or just socket not connected to host? send data directly to Atari
    {
        onIkdbDisabled();
    }
}
