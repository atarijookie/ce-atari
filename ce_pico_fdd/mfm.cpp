#include "WiFi.h"

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "display.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"

#define MFM_BUFFER_SIZE         512   // half of mfmBuffer is 256 items, which gives about 1 ms (256 items * 4 us per item) of time to refill half before the other half is used
#define MFM_BUFFER_HALF_SIZE    (MFM_BUFFER_SIZE / 2)

__attribute__((aligned(512))) uint16_t mfmBuffer[MFM_BUFFER_SIZE];

int dmaChannel;
volatile bool halfDone = false;

void __isr dmaHandlerMfm(void);

void setupPwmOutput(void)
{
  gpio_set_function(PIN_RDATA, GPIO_FUNC_PWM);
  uint pwmSliceNum = pwm_gpio_to_slice_num(PIN_RDATA); 
  uint pwmChannel = pwm_gpio_to_channel(PIN_RDATA);
 
  pwm_set_clkdiv_int_frac(pwmSliceNum, 75, 0);      // 150 MHz / 75 = 2 MHz -- 1 tick is 0.5 us
  pwm_set_output_polarity(pwmSliceNum, true, true);

  pwm_set_wrap(pwmSliceNum, 7);                     // Set period (timer going from from 0 to this value)
  pwm_set_chan_level(pwmSliceNum, pwmChannel, 1);   // Set channel output high for one cycle before dropping

  pwm_set_enabled(pwmSliceNum, true);               // Set the PWM running
}

void setupDmaToPwm(void)
{
    for (int i = 0; i < MFM_BUFFER_SIZE; i++) {
        mfmBuffer[i] = 7;
    }

    uint pwmChannel = pwm_gpio_to_channel(PIN_RDATA);
    uint pwmSliceNum = pwm_gpio_to_slice_num(PIN_RDATA); 

    dmaChannel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dmaChannel);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);     // Transfer size: 16 bits
    channel_config_set_read_increment(&c, true);                // Source increments through mfmBuffer
    channel_config_set_write_increment(&c, false);              // Destination is fixed (PWM CC register)
    channel_config_set_dreq(&c, pwm_get_dreq(pwmSliceNum));     // Pace by PWM slice DREQ (fires at counter wrap)
    channel_config_set_ring(&c, false, 10);                     // enable ring mode, on read side, after 1024 bytes (== 1 << 10) (because we have 512 uint16_t items in buffer)

    // dma_encode_endless_transfer_count() vs MFM_BUFFER_HALT_SIZE
    dma_channel_configure(dmaChannel, &c, (volatile void *) 
                            (&pwm_hw->slice[pwmSliceNum].top),
                            mfmBuffer, 
                            dma_encode_transfer_count_with_self_trigger(MFM_BUFFER_HALF_SIZE), 
                            true);

    // Enable DMA interrupt
    dma_channel_set_irq0_enabled(dmaChannel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dmaHandlerMfm);
    irq_set_enabled(DMA_IRQ_0, true);
}

// DMA IRQ handler
void __isr dmaHandlerMfm(void)
{
    // Clear the interrupt
    dma_hw->ints0 = 1u << dmaChannel;

    // Flip half flag
    halfDone = !halfDone;

    if (halfDone)   // First half finished
    {
        

    }
    else            // Second half finished
    {
        

    }
}
