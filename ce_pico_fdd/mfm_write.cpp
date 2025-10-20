#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "mfm_write.pio.h"
#include "pico/util/queue.h"
#include "hardware/dma.h"

#include "defs.h"

#define MFM_4US         1
#define MFM_6US         2
#define MFM_8US         3

// exact expected pulses would be: 23 ticks, 43 ticks, 63 ticks
#define PULSE_TOO_SHORT 13
#define PULSE_4US       33
#define PULSE_6US       53
#define PULSE_8US       73

extern queue_t fifoMfmWrite;

static PIO pioMfmWrite;
static uint smMfmWrite;
static uint offsetMfmWrite;

static uint dma_channel_rx;

#define DMA_IRQ_TO_USE  1

#define MFM_WRITE_BUFFER_SIZE   8
#define MFM_WRITE_BUFFER_HALF_SIZE    (MFM_WRITE_BUFFER_SIZE / 2)

__attribute__((aligned(32))) uint32_t mfmBufferWrite[MFM_WRITE_BUFFER_SIZE]; // must align for ring mode to work correctly

volatile bool halfDoneWrite = false;
volatile uint8_t wrStreamByte = 0;
volatile uint8_t wrBits = 0;

extern TWriteBuffer wrBuffer;  // buffer for written sectors

// take one captured value and turn it into symbol duration
void updateWriteDataDirect(uint32_t capturedStamp)
{
    uint32_t capturedDuration = 0xffffffff - capturedStamp;  // calculate the change from start value of X register

    uint8_t newTime = 0;

    if(capturedDuration < PULSE_TOO_SHORT) {    // if this pulse is too short (less than 2.7 us long)
        return;
    } else if(capturedDuration < PULSE_4US) {   // 4 us? (interval 2.7 us - 5.0 us) (40 pulses of 0.125 us)
        newTime = MFM_4US;
    } else if(capturedDuration < PULSE_6US) {   // 6 us? (interval 5.0 us - 7.0 us) (56 pulses of 0.125 us)
        newTime = MFM_6US;
    } else if(capturedDuration < PULSE_8US) {   // 8 us? (interval 7.0 us - 9.0 us) (72 pulses of 0.125 us)
        newTime = MFM_8US;
    } else {                            // pulse too long? (longer than 9 us)
        return;
    }

    wrStreamByte = (wrStreamByte << 2) | newTime;   // append new time to streamByte
    wrBits += 2;            // now got 2 more bits

    if(wrBits >= 8) {       // got 8 bits?
        wrBits = 0;         // don't have bits now
        queue_try_add(&fifoMfmWrite, (const void*) &wrStreamByte);
    }
}

// DMA interrupt handler, called when a DMA channel has data
static void dmaHandlerMfmWrite(void)
{
    if (dma_channel_rx >= 0 && dma_irqn_get_channel_status(DMA_IRQ_TO_USE, dma_channel_rx)) {
        dma_irqn_acknowledge_channel(DMA_IRQ_TO_USE, dma_channel_rx);

        halfDoneWrite = !halfDoneWrite;   // Flip half flag

        uint32_t* bfr = halfDoneWrite ? &mfmBufferWrite[0] : &mfmBufferWrite[MFM_WRITE_BUFFER_HALF_SIZE];

        // process half of the write buffer
        for(int i=0; i<MFM_WRITE_BUFFER_HALF_SIZE; i++) {
            updateWriteDataDirect(bfr[i]);
        }
    }
}

void pio_mfm_write_setup(void)
{
    // This will find a free pio and state machine for our program and load it for us
    // We use pio_claim_free_sm_and_add_program_for_gpio_range (for_gpio_range variant)
    // so we will get a PIO instance suitable for addressing gpios >= 32 if needed and supported by the hardware
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&mfm_write_program, &pioMfmWrite, &smMfmWrite, &offsetMfmWrite, PIN_WDATA, 1, true);
    hard_assert(success);

    mfm_write_program_init(pioMfmWrite, smMfmWrite, offsetMfmWrite, PIN_WDATA);

    // Setup dma for write
    dma_channel_rx = dma_claim_unused_channel(true);
    dma_channel_config config_rx = dma_channel_get_default_config(dma_channel_rx);

    channel_config_set_transfer_data_size(&config_rx, DMA_SIZE_32);
    channel_config_set_read_increment(&config_rx, false);
    channel_config_set_write_increment(&config_rx, true);
    channel_config_set_dreq(&config_rx, pio_get_dreq(pioMfmWrite, smMfmWrite, false));  // setup dma to read from pio fifo
    channel_config_set_ring(&config_rx, true, 5);         // enable ring mode, on write side, 5 address bits masked for ring mode (32 bytes == 8 dwords)

    // enable irq for rx
    dma_irqn_set_channel_enabled(DMA_IRQ_TO_USE, dma_channel_rx, true);

    // 32-bit read from the FIFO
    dma_channel_configure(  dma_channel_rx, &config_rx,
                            mfmBufferWrite,
                            &pioMfmWrite->rxf[smMfmWrite],
                            dma_encode_transfer_count_with_self_trigger(MFM_WRITE_BUFFER_HALF_SIZE),
                            true); // dma started

    // enable DMA IRQ handler
    dma_channel_set_irq1_enabled(dma_channel_rx, true);
    irq_set_exclusive_handler(dma_get_irq_num(DMA_IRQ_TO_USE), dmaHandlerMfmWrite);
    irq_set_enabled(dma_get_irq_num(DMA_IRQ_TO_USE), true);
}
