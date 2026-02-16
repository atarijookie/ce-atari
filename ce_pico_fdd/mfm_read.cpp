#include <stdio.h>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "display.h"
#include "psram.h"

volatile bool core1running = false;

#define MFM_BUFFER_SIZE         2048                                // 12 address bits masked for ring mode (4096 bytes == 2048 words)
#define MFM_BUFFER_HALF_SIZE    (MFM_BUFFER_SIZE / 2)
#define MFM_READ_SIZE_FILLS     (MFM_BUFFER_HALF_SIZE / 4)

__attribute__((aligned(4096))) uint16_t mfmBuffer[MFM_BUFFER_SIZE]; // must align to 4096 for ring mode to work correctly

int dmaChannel;
volatile bool halfDone = false;

#define FILL_NONE   0
#define FILL_LOWER  1
#define FILL_UPPER  2

volatile uint8_t fillWhat = FILL_NONE;

extern uint8_t trackData0[READTRACKDATA_SIZE_BYTES];
extern uint8_t trackData1[READTRACKDATA_SIZE_BYTES];
uint32_t dataIndexInTrack = STREAM_START_OFFSET;
volatile uint32_t lastStepTime = 0;
volatile uint32_t timeTrackStart;

extern TDrivePosition posStreamed, hwPosition, posWritten;

void readTrackData_goToStart(void);

extern queue_t fifoToCore0;
extern queue_t fifoToCore1;

void __isr dmaHandlerMfm(void);

// interrupt handler for STEP signal
void __isr floppyStepISR(uint gpio, uint32_t event_mask)
{
    uint32_t now = millis();

    if((now - lastStepTime) < 1) {  // last step ISR was less than 2 ms ago? this is a glitch, ignore it
        return;
    }
    lastStepTime = now;

    if(BIT_IS_H(PIN_MOT_EN)) {       // motor not enabled? Skip the following code.
        return;
    }

    if(BIT_IS_H(PIN_DIR)) {  // direction is High? track--
        if(hwPosition.track > 0) {
            hwPosition.track--;
        }
    } else  {                // direction is Low? track++
        if(hwPosition.track < MAX_TRACKS) {
            hwPosition.track++;
        }
    }

    if(hwPosition.track == 0) {   // if track is 0, TRACK00 is L
        gpio_put(PIN_TRACK00, 0);
    } else {                        // if track is not 0, TRACK00 to H
        gpio_put(PIN_TRACK00, 1);
    }
}

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
    uint pwmChannel = pwm_gpio_to_channel(PIN_RDATA);
    uint pwmSliceNum = pwm_gpio_to_slice_num(PIN_RDATA);

    dmaChannel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dmaChannel);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);     // Transfer size: 16 bits
    channel_config_set_read_increment(&c, true);                // Source increments through mfmBuffer
    channel_config_set_write_increment(&c, false);              // Destination is fixed (PWM CC register)
    channel_config_set_dreq(&c, pwm_get_dreq(pwmSliceNum));     // Pace by PWM slice DREQ (fires at counter wrap)
    channel_config_set_ring(&c, false, 12);                     // enable ring mode, on read side, 12 address bits masked for ring mode (4096 bytes == 2048 words)

    // dma_encode_endless_transfer_count() vs MFM_BUFFER_HALF_SIZE
    dma_channel_configure(  dmaChannel, &c,
                            (volatile void *) (&pwm_hw->slice[pwmSliceNum].top),
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

    halfDone = !halfDone;   // Flip half flag

    fillWhat = halfDone ? FILL_LOWER : FILL_UPPER;
}

void getMfmDataToBuffer(uint8_t* bfr, int len)
{
    // update SIDE var
    hwPosition.side = BIT_IS_H(PIN_SIDE1) ? 0 : 1; // get the current SIDE

    // get current track and side we should be streaming, limit them to maximum values
    int trackNo = MIN(hwPosition.track, MAX_TRACKS);
    int sideNo = MIN(hwPosition.side, 1);

    uint8_t* pTrackDataStart = (sideNo == 0) ? trackData0 : trackData1;
    uint8_t* pTrackData = &pTrackDataStart[dataIndexInTrack];  // copy data from here
    uint8_t* pTrackDataEnd = &pTrackDataStart[READTRACKDATA_SIZE_BYTES - 1];

    memset(bfr, 0x55, len);                // init all values to 0x55

    for(int i=0; i<len; ) {
        uint8_t val = *pTrackData;

        // end of array or end-of-track marker? we've at the end, don't copy anything more
        if(pTrackData >= pTrackDataEnd || val == CMD_TRACK_STREAM_END) {
            break;
        }

        pTrackData++;

        // skip empty bytes
        if(val == 0) {
            continue;
        }

        // skip data part of the sector marker
        if(val == CMD_DATA_PART_OF_SECTOR) {
            continue;
        }

        // current sector marker?
        if(val == CMD_CURRENT_SECTOR) {
            posStreamed.side = *pTrackData++;
            posStreamed.track = *pTrackData++;
            posStreamed.sector = *pTrackData++;
            continue;
        }

        // val is just mfm data, store it
        bfr[i] = val;
        i++;
    }

    // now we got 'len' bytes in the bfr, we just need to update data retrieval index
    dataIndexInTrack = pTrackData - pTrackDataStart;
}

const uint16_t arrValues[4] = {7, 7, 11, 15};       // conversion table from mfm packed symbol to timer ARR value (for 0 us, 4 us, 6 us, 8 us)

void fillHalfMfmBuffer(uint8_t what)
{
    uint16_t* bfr = (what == FILL_LOWER) ? &mfmBuffer[0] : &mfmBuffer[MFM_BUFFER_HALF_SIZE];

    // from the track buffer (with all the additional data and spaces) extract
    // just MFM_READ_SIZE_FILLS bytes which can be transformed into MFM intervals
    uint8_t rawMfmData[MFM_READ_SIZE_FILLS];
    getMfmDataToBuffer(rawMfmData, MFM_READ_SIZE_FILLS);

    for(int i=0; i<MFM_READ_SIZE_FILLS; i++) {
        // get one byte with 4 intervals
        uint8_t streamByte = rawMfmData[i];

        // unpack intervals into 4 items of uint16_t
        bfr[0] = arrValues[ ((streamByte >> 6) & 3) ];
        bfr[1] = arrValues[ ((streamByte >> 4) & 3) ];
        bfr[2] = arrValues[ ((streamByte >> 2) & 3) ];
        bfr[3] = arrValues[ ((streamByte     ) & 3) ];

        bfr += 4;
    }
}

void clearMfmBuffer(void)
{
    // copy 'all 4 us' pulses into current streaming buffer to allow shortest possible switch to start of track
    for(int i=0; i<MFM_BUFFER_SIZE; i++) {
        mfmBuffer[i] = 11;
    }
}

void readTrackData_goToStart(void)
{
    dataIndexInTrack = STREAM_START_OFFSET;     // stream index to start
    timeTrackStart = millis();                  // time of track start to now
}

void requestTrackLoad(uint16_t track, uint16_t sector);

void updateStreamPositionByFloppyPosition(void)
{
    uint32_t now = millis();
    uint32_t timeSinceTrackStart = now - timeTrackStart;
    int currentSectorIndex = (timeSinceTrackStart / 18);        // convert timeSinceTrackStart to sector index (0 - 10)
    int currentSectorNo = MIN(currentSectorIndex + 1, MAX_SECTORS_PER_TRACK);   // limit current sector number

    // from the stream table read offset to this sector
    uint32_t currentSectorStartIndex = getWord(trackData0 + (currentSectorNo * 2));
    dataIndexInTrack = MIN(currentSectorStartIndex, READTRACKDATA_SIZE_BYTES-1);
}

enum MfmStreamState {
  STATE_STREAMING,          // no step occured recently, we can just stream
  STATE_STEPPING,           // step happened less than 15 ms ago, there might be more, don't stream
  STATE_LOADING             // we're now loading data from PSRAM to SRAM, once this is done we can start streaming
};

void core1_main_loop(void)
{
    flash_safe_execute_core_init();     // call this for flash_safe_execute() to work

    core1running = true;

    clearMfmBuffer();
    readTrackData_goToStart();

    gpio_set_irq_enabled_with_callback(PIN_STEP, GPIO_IRQ_EDGE_FALL, true, floppyStepISR);

    // start mfm output
    setupPwmOutput();
    setupDmaToPwm();

    MfmStreamState state = STATE_STEPPING;

    uint32_t tReq = 0, tStr = 0;

    while(1)
    {
        uint32_t now = millis();
        uint32_t timeMsSinceLastStep = now - lastStepTime;

        if(timeMsSinceLastStep < 15) {      // last STEP happened within last 15 ms?
            if(state != STATE_STEPPING) {   // we weren't STEPPING before this? clear mfm buffer
                clearMfmBuffer();
            }

            state = STATE_STEPPING;         // we're in the stepping state
        }
        else
        {   // last step happened at least 15 ms ago? we're not stepping anymore
            if(state == STATE_STEPPING) {
                queue_try_add(&fifoToCore0, (const void*) &hwPosition.track);
                state = STATE_LOADING;
                tReq = millis();
            }
        }

        uint8_t trackGot = 0xff;
        while(!queue_is_empty(&fifoToCore1)) {
            queue_try_remove(&fifoToCore1, &trackGot);
        }
        if(trackGot == hwPosition.track) {
            state = STATE_STREAMING;
            tStr = millis();
            // debug("r->s %d\n", tStr - tReq);

            updateStreamPositionByFloppyPosition();
        }

        // MFM read buffer should be refilled?
        if(fillWhat != FILL_NONE) {
            uint8_t whatCopy = fillWhat;    // make a copy of global var, so we can clear global var before entering fillHalfMfmBuffer
            fillWhat = FILL_NONE;

            if(state == STATE_STREAMING) {
                fillHalfMfmBuffer(whatCopy);
            }
        }

        // index pulse generating and stream restart
        now = millis();
        uint32_t timeSinceTrackStart = now - timeTrackStart;

        gpio_put(PIN_INDEX, (timeSinceTrackStart <= 195) ? 1 : 0);  // INDEX is H for time 0-195, index L for times 196-200

        if(timeSinceTrackStart >= 200) {    // track finished
            readTrackData_goToStart();      // move the pointer in the track stream to start
        }
    }
}
