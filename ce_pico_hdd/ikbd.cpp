#include <Ethernet.h>

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "ikbd.h"
#include "uart_rx.pio.h"

extern String hostIpString;
extern uint16_t hostPortIkbd;
extern bool connected;              // true if connected
extern bool ikbdConnected;

extern EthernetClient clientIkbd;

#define IKBD_BFR_SIZE 128
uint8_t buffer[IKBD_BFR_SIZE];

static PIO pioUart;
static uint smUart;
static int8_t pioIrqUart;
static queue_t fifoUart;

void onIkdbDisabled(void)
{
    // ikbd not sending data (chip not present) or ikbd not enabled, but the ikbd socket is connected, then disconnect
    // if any data comming from host via socket is available, read and and drop it
    while(clientIkbd.available() > 0)
    {
        int available = clientIkbd.available();
        int readSize = MIN(available, IKBD_BFR_SIZE);
        clientIkbd.read(buffer, readSize);
    }

    while(Serial1.available() > 0)          // got data from KEYB_TX_ORIG? just send it back to KEYB_TX
    {
        uint8_t data = Serial1.read();
        Serial1.write(data);
    }

    while(!queue_is_empty(&fifoUart)) {     // got data from Atari? Just read it and ignore it
        uint8_t c;
        queue_try_remove(&fifoUart, &c);
    }
}

void onIkbdEnabled(void)
{
    int readSize;
    uint8_t data[2];

    // keep sending forwarding data around until all the sources are empty
    while(Serial1.available() || !queue_is_empty(&fifoUart) || clientIkbd.available())
    {
        if(Serial1.available() > 0)     // got data from KEYB_TX_ORIG? send it to host with tag
        {
            data[0] = UARTMARK_KEYBDATA;
            data[1] = Serial1.read();
            clientIkbd.write(data, 2);
        }

        if(!queue_is_empty(&fifoUart))     // got data from KEYB_RX? send it to host with tag
        {
            data[0] = UARTMARK_STCMD;
            queue_try_remove(&fifoUart, &data[1]);
            clientIkbd.write(data, 2);
        }

        if(clientIkbd.available() > 0)  // got data from host? send it to Atari
        {
            data[0] = clientIkbd.read();
            Serial1.write(data[0]);
        }
    }
}

void ikbdConnectDisconnect(void)
{
    if(settings.ikbdEnabled)
    {
        // ikbd is enabled, ikdb chip is sending data (chip present), eth is connected, but our ikbd socket is NOT connected, connect now
        if(connected && !ikbdConnected)
        {
            debug("I connect");
            ikbdConnected = clientIkbd.connect(hostIpString.c_str(), hostPortIkbd);
            // clientIkbd.setNoDelay(true);
        }
    }
    else
    {
        // ikbd not sending data (chip not present) or ikbd not enabled, but the ikbd socket is connected, then disconnect
        if(ikbdConnected)
        {
            debug("I disconnect");
            ikbdConnected = false;
            clientIkbd.stop();
        }
    }
}

void ikbdProcessing(void)
{
    static uint32_t lastStatus = 0;
    static uint32_t lastRun = 0;

    uint32_t now = millis();
    if(now - lastRun < 20) {            // process only once in a while
        return;
    }
    lastRun = now;

    if((now - lastStatus) >= 1000) {    // once per second
        lastStatus = now;

        if(ikbdConnected) {             // if connected, send ALIVE mark and value every second
            uint8_t data[2] = {UARTMARK_ALIVE, UARTMARK_ALIVE};
            clientIkbd.write(data, 2);
        }
    }

    ikbdConnectDisconnect();

    // ikbd enabled and ikbd socket is connected? send and get data to/from host
    if(settings.ikbdEnabled && ikbdConnected) {
        onIkbdEnabled();
    } else {   // ikbd disabled or just socket not connected to host? send data directly to Atari
        onIkdbDisabled();
    }
}

// IRQ called when the pio fifo is not empty, i.e. there are some characters on the uart
static void pio_irq_func(void) {
    while(!pio_sm_is_rx_fifo_empty(pioUart, smUart)) {
        char c = uart_rx_program_getc(pioUart, smUart);
        if (!queue_try_add(&fifoUart, &c)) {
            debug("fifo full\n");
        }
    }
}

void ikbdInit(void)
{
    Serial1.begin(IKBD_BAUD_RATE);

    queue_init(&fifoUart, 1, IKBD_BFR_SIZE);

    uint offset;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_rx_program, &pioUart, &smUart, &offset, PIN_KEYB_RX, 1, true);
    if(!success) { debug("Failed to claim PIO SM for UART\n"); while(1); }
    uart_rx_program_init(pioUart, smUart, offset, PIN_KEYB_RX, IKBD_BAUD_RATE);

    // Find a free irq
    pioIrqUart = pio_get_irq_num(pioUart, 0);
    if (irq_get_exclusive_handler(pioIrqUart)) {
        pioIrqUart++;
        if (irq_get_exclusive_handler(pioIrqUart)) {
            debug("All IRQs are in use\n");
        }
    }

    // Enable interrupt
    irq_add_shared_handler(pioIrqUart, pio_irq_func, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
    irq_set_enabled(pioIrqUart, true); // Enable the IRQ
    const uint irq_index = pioIrqUart - pio_get_irq_num(pioUart, 0); // Get index of the IRQ
    pio_set_irqn_source_enabled(pioUart, irq_index, pio_get_rx_fifo_not_empty_interrupt_source(smUart), true); // Set pio to tell us when the FIFO is NOT empty
}
