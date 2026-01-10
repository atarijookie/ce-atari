#include <Ethernet.h>

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "ikbd.h"
#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

extern String hostIpString;
extern uint16_t hostPortIkbd;
extern volatile bool connected;              // true if connected
extern volatile bool ikbdConnected;

extern EthernetClient clientIkbd;

#define IKBD_BFR_SIZE 128
uint8_t buffer[IKBD_BFR_SIZE];

static PIO pioUartKeybRx;
static uint smUartKeybRx;
static queue_t fifoUartKeybRx;

static PIO pioUartKeybTxOrig;
static uint smUartKeybTxOrig;
static queue_t fifoUartKeybTxOrig;

static PIO pioUartKeybTx;
static uint smUartKeybTx;
static queue_t fifoUartKeybTx;

static int8_t pioIrqUartRx;

void onIkdbDisabled(void)
{
    uint8_t data;

    // ikbd not sending data (chip not present) or ikbd not enabled, but the ikbd socket is connected, then disconnect
    // if any data comming from host via socket is available, read and and drop it
    while(clientIkbd.available() > 0)
    {
        int available = clientIkbd.available();
        int readSize = MIN(available, IKBD_BFR_SIZE);
        clientIkbd.read(buffer, readSize);
    }

    // got data from KEYB_TX_ORIG? just send it back to KEYB_TX
    while(!queue_is_empty(&fifoUartKeybTxOrig))
    {
        queue_try_remove(&fifoUartKeybTxOrig, &data);
        queue_try_add(&fifoUartKeybTx, &data);
    }

    // got data from Atari? Just read it and ignore it
    while(!queue_is_empty(&fifoUartKeybRx)) {
        queue_try_remove(&fifoUartKeybRx, &data);
    }

    // got data for KEYB_TX and can send without blocking? send it
    while(!queue_is_empty(&fifoUartKeybTx) && !pio_sm_is_tx_fifo_full(pioUartKeybTx, smUartKeybTx)) {
        queue_try_remove(&fifoUartKeybTx, &data);
        uart_tx_program_putc(pioUartKeybTx, smUartKeybTx, data);
    }
}

void onIkbdEnabled(void)
{
    int readSize;
    uint8_t data[2];

    // keep sending forwarding data around until all the sources are empty
    while(1)
    {
        // Set processing to false, and if anything has been received / sent from / to uart, then we're still processing.
        bool processing = false;

        if(!queue_is_empty(&fifoUartKeybTxOrig))     // got data from KEYB_TX_ORIG? send it to host with tag
        {
            data[0] = UARTMARK_KEYBDATA;
            queue_try_remove(&fifoUartKeybTxOrig, &data[1]);
            clientIkbd.write(data, 2);

            processing = true;  // still processing data in this loop iteration
        }

        if(!queue_is_empty(&fifoUartKeybRx))     // got data from KEYB_RX? send it to host with tag
        {
            data[0] = UARTMARK_STCMD;
            queue_try_remove(&fifoUartKeybRx, &data[1]);
            clientIkbd.write(data, 2);

            processing = true;  // still processing data in this loop iteration
        }

        if(clientIkbd.available() > 0)  // got data from host? send it to Atari
        {
            data[0] = clientIkbd.read();
            queue_try_add(&fifoUartKeybTx, &data[0]);

            processing = true;  // still processing data in this loop iteration
        }

        // got data for KEYB_TX and can send without blocking? send it
        if(!queue_is_empty(&fifoUartKeybTx) && !pio_sm_is_tx_fifo_full(pioUartKeybTx, smUartKeybTx)) {
            queue_try_remove(&fifoUartKeybTx, &data[0]);
            uart_tx_program_putc(pioUartKeybTx, smUartKeybTx, data[0]);

            processing = true;  // still processing data in this loop iteration
        }

        // no UART needed processing in this loop, we can quit this loop now
        if(!processing) {
            break;
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
            debug("I con\n");
            ikbdConnected = clientIkbd.connect(hostIpString.c_str(), hostPortIkbd);
            // clientIkbd.setNoDelay(true);
        }
    }
    else
    {
        // ikbd not sending data (chip not present) or ikbd not enabled, but the ikbd socket is connected, then disconnect
        if(ikbdConnected)
        {
            debug("I dis\n");
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
static void pio_irq_func(void)
{
    // for KEYB_RX
    while(!pio_sm_is_rx_fifo_empty(pioUartKeybRx, smUartKeybRx)) {
        char c = uart_rx_program_getc(pioUartKeybRx, smUartKeybRx);
        if (!queue_try_add(&fifoUartKeybRx, &c)) {
            debug("fifo full\n");
        }
    }

    // for KEYB_TX_ORIG
    while(!pio_sm_is_rx_fifo_empty(pioUartKeybTxOrig, smUartKeybTxOrig)) {
        char c = uart_rx_program_getc(pioUartKeybTxOrig, smUartKeybTxOrig);
        if (!queue_try_add(&fifoUartKeybTxOrig, &c)) {
            debug("fifo full\n");
        }
    }
}

void ikbdInit(void)
{
    queue_init(&fifoUartKeybRx, 1, IKBD_BFR_SIZE);
    queue_init(&fifoUartKeybTxOrig, 1, IKBD_BFR_SIZE);
    queue_init(&fifoUartKeybTx, 1, IKBD_BFR_SIZE);

    uint offset;
    bool success;

    // for KEYB_TX
    success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_tx_program, &pioUartKeybTx, &smUartKeybTx, &offset, PIN_KEYB_TX, 1, true);
    if(!success) { debug("HALT! Failed to claim PIO SM for UART\n"); while(1); }
    uart_tx_program_init(pioUartKeybTx, smUartKeybTx, offset, PIN_KEYB_TX, IKBD_BAUD_RATE);

    // for KEYB_RX
    success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_rx_program, &pioUartKeybRx, &smUartKeybRx, &offset, PIN_KEYB_RX, 1, true);
    if(!success) { debug("HALT! Failed to claim PIO SM for UART\n"); while(1); }
    uart_rx_program_init(pioUartKeybRx, smUartKeybRx, offset, PIN_KEYB_RX, IKBD_BAUD_RATE);

    // for KEYB_TX_ORIG
    success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_rx_program, &pioUartKeybTxOrig, &smUartKeybTxOrig, &offset, PIN_KEYB_TX_ORIG, 1, true);
    if(!success) { debug("HALT! Failed to claim PIO SM for UART\n"); while(1); }
    uart_rx_program_init(pioUartKeybTxOrig, smUartKeybTxOrig, offset, PIN_KEYB_TX_ORIG, IKBD_BAUD_RATE);

    // Find a free irq
    pioIrqUartRx = pio_get_irq_num(pioUartKeybRx, 0);
    if (irq_get_exclusive_handler(pioIrqUartRx)) {
        pioIrqUartRx++;
        if (irq_get_exclusive_handler(pioIrqUartRx)) {
            debug("HALT! All IRQs are in use\n");
            while(1);
        }
    }

    // Enable interrupt
    irq_add_shared_handler(pioIrqUartRx, pio_irq_func, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
    irq_set_enabled(pioIrqUartRx, true); // Enable the IRQ
    const uint irq_index = pioIrqUartRx - pio_get_irq_num(pioUartKeybRx, 0); // Get index of the IRQ

    // Set pio to tell us when the FIFO is NOT empty
    pio_set_irqn_source_enabled(pioUartKeybRx, irq_index, pio_get_rx_fifo_not_empty_interrupt_source(smUartKeybRx), true);
    pio_set_irqn_source_enabled(pioUartKeybTxOrig, irq_index, pio_get_rx_fifo_not_empty_interrupt_source(smUartKeybTxOrig), true);
}
