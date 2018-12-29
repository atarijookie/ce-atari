#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#include "defs.h"
#include "timers.h"
#include "main.h"
#include "floppyhelpers.h"
#include "init.h"

TWriteBuffer wrBuffer[2];                           // two buffers for written sectors
TWriteBuffer *wrNow;

SStreamed streamed;

WORD mfmReadStreamBuffer[16];                           // 16 words - 16 mfm times. Half of buffer is 8 times - at least 32 us (8 * 4us),

WORD mfmWriteStreamBuffer[16];
//WORD lastMfmWriteTC;

// cycle measure: t1 = TIM3->CNT;   t2 = TIM3->CNT; dt = t2 - t1; -- subtrack 0x12 because that's how much measuring takes
WORD t1, t2, dt;

// digital osciloscope time measure on GPIO B0:
//          GPIOB->BSRR = 1;    // 1
//          GPIOB->BRR = 1;     // 0

/* Franz to host communication:
A) send   : ATN_SEND_TRACK with the track # and side # -- 2 WORDs + zeros = 3 WORDs
   receive: track data with sector start marks, up to 15 kB -- 12 sectors + the marks

B) send   : ATN_SECTOR_WRITTEN with the track, side, sector # + captured data, up to 1500 B
   receive: nothing (or don't care)

C) send   : ATN_FW_VERSION with the FW version + empty bytes == 3 WORD for FW + empty WORDs
   receive: commands possibly received -- receive 6 WORDs (3 empty, 3 with possible commands)
*/

//--------------
// circular buffer
// watch out, these macros take 0.73 us for _add, and 0.83 us for _get operation!

//#define     wrBuffer_add(X)                 { if(wrNow->count  < WRITEBUFFER_SIZE) { wrNow->buffer[wrNow->count] = X; wrNow->count++; } }

#define     readTrackData_goToStart()       { inIndexGet = streamStartOffset; }
#define     readTrackData_get(X)            { X = readTrackData[inIndexGet];                inIndexGet++;       if(inIndexGet >= READTRACKDATA_SIZE) {  inIndexGet = streamStartOffset; }; }
#define     readTrackData_get_noMove(X)     { X = readTrackData[inIndexGet];                                                                                                                                    }
//#define       readTrackData_markAndmove() { readTrackData[inIndexGet] = CMD_MARK_READ;    inIndexGet++;       if(inIndexGet >= READTRACKDATA_SIZE) {  inIndexGet = 0; };   }
#define     readTrackData_justMove()        {                                                                                       inIndexGet++;       if(inIndexGet >= READTRACKDATA_SIZE) { inIndexGet = streamStartOffset; };   }
//--------------

#define REQUEST_TRACK                       {   next.track = now.track; next.side = now.side; sendTrackRequest = TRUE; lastRequestTime = TIM4->CNT; trackStreamedCount = 0; }
#define FORCE_REQUEST_TRACK                 {   REQUEST_TRACK;      lastRequestTime -= 35;      lastRequested.track = 0xff;     lastRequested.side = 0xff;                  }

WORD version[2] = {0xf018, 0x1205};             // this means: Franz, 2018-12-05
WORD drive_select;

volatile BYTE sendFwVersion, sendTrackRequest;

WORD atnSendFwVersion       [ATN_SENDFWVERSION_LEN_TX];
BYTE cmdBuffer              [ATN_SENDFWVERSION_LEN_RX * 2];

WORD atnSendTrackRequest    [ATN_SENDTRACK_REQ_LEN_TX];
BYTE readTrackData          [READTRACKDATA_SIZE];
WORD inIndexGet;

WORD fakeBuffer;

volatile WORD prevIntTime;

volatile BYTE spiDmaIsIdle;
volatile BYTE spiDmaTXidle, spiDmaRXidle;       // flags set when the SPI DMA TX or RX is idle

volatile TDrivePosition now, next, lastRequested, prev;
volatile WORD lastRequestTime;

BYTE hostIsUp;                                  // used to just pass through IKBD until RPi is up

BYTE driveId;
BYTE driveEnabled;

BYTE isDiskChanged;
BYTE isWriteProtected;

WORD trackStreamedCount = 0;

extern volatile TCircBuffer buff0, buff1;
void circularInit(volatile TCircBuffer *cb);

void setupDriveSelect(void);
void setupDiskChangeWriteProtect(void);

void fillReadStreamBufferWithDummyData(void);
void fillMfmTimesWithDummy(void);
TOutputFlags outFlags;

void updateStreamPositionByFloppyPosition(void);

void handleFloppyWrite(void);

BYTE sectorsWritten;            // how many sectors were written during the last media rotation - if something was written, we need to get the re-encoded track
WORD wrPulseShort, wrPulseLong; // if write pulse is too short of too long, increment here

WORD streamTableOffset = 10;              // the stream table starts at this offset, because first 5 words are empty (ATN + sizes + other)
WORD streamStartOffset = 10 + STREAM_TABLE_SIZE;
void findStreamTableOffset(void);

int main (void)
{
    BYTE indexCount     = 0;
    WORD WGate;
    BYTE spiDmaIsIdle   = TRUE;

    prevIntTime = 0;
    sectorsWritten = 0;         // nothing written yet

    sendFwVersion       = FALSE;
    sendTrackRequest    = FALSE;

    setupAtnBuffers();
    init_hw_sw();                                   // init GPIO pins, timers, DMA, global variables

    circularInit(&buff0);
    circularInit(&buff1);

    setupDriveSelect();                             // the drive select bits which should be LOW if the drive is selected

    // init floppy signals
    GPIOB->BSRR = (WR_PROTECT | DISK_CHANGE);       // not write protected
    GPIOB->BRR = TRACK0 | DISK_CHANGE;              // TRACK 0 signal to L, DISK_CHANGE to LOW
    GPIOB->BRR = ATN;                               // ATTENTION bit low - nothing to read

    setupDiskChangeWriteProtect();                  // init write-protect and disk-change pins

    REQUEST_TRACK;                                  // request track 0, side 0

    while(1) {
        WORD inputs;

        if(sendTrackRequest) {                          // if we're already waiting for the new TRACK to be sent, consider this as we are already receiving new track data
            fillReadStreamBufferWithDummyData();        // fill MFM stream with dummy data so we won't stream any junk
            outFlags.weAreReceivingTrack = TRUE;        // mark that we are receiving TRACK data, and thus shouldn't stream
        }

        if(outFlags.updatePosition) {                   // this happens when we received new track and we need to update our position in read stream
            outFlags.updatePosition = FALSE;

            findStreamTableOffset();                    // stream table has mostly 10 bytes offset, but could be one word more or less

            updateStreamPositionByFloppyPosition();     // place the read marker on the right place in the stream
        }

        // ST wants the stream and we are not receiving TRACK data? ENABLE stream
        if(outFlags.stWantsTheStream && !outFlags.weAreReceivingTrack) {
            if(!outFlags.outputsAreEnabled) {           // the outputs are not enabled yet?
                FloppyOut_Enable();                     // enable them
                outFlags.outputsAreEnabled = TRUE;      // mark that we enabled them
            }
        } else {    // other cases? DISABLE stream
            if(outFlags.outputsAreEnabled) {            // the outputs are enabled?
                FloppyOut_Disable();                    // disable them
                outFlags.outputsAreEnabled = FALSE;     // mark that we disabled them
            }
        }

        // sending and receiving data over SPI using DMA
        if(spiDmaIsIdle == TRUE) {                                                              // SPI DMA: nothing to Tx and nothing to Rx?
            if(sendFwVersion) {                                                                 // should send FW version? this is a window for receiving commands
                if(isDiskChanged) {                                                             // if it was held for a while, take it down
                    isDiskChanged = FALSE;
                    setupDiskChangeWriteProtect();
                }

                timeoutStart();                                                                 // start a time-out timer

                spiDma_txRx(ATN_SENDFWVERSION_LEN_TX, (BYTE *) &atnSendFwVersion[0], ATN_SENDFWVERSION_LEN_RX, (BYTE *) &cmdBuffer[0]);

                sendFwVersion   = FALSE;
            } else if(sendTrackRequest) {                                                       // if should send track request
                // check how much time passed since the request was created
                WORD timeNow    = TIM4->CNT;
                WORD diff       = timeNow - lastRequestTime;

                if(diff >= 30) {                                                                // and at least 15 ms passed since the request (30 / 2000 s)
                    sendTrackRequest    = FALSE;

                    // first check if this isn't what we've requested last time
                    if(next.track != lastRequested.track || next.side != lastRequested.side) {  // if track or side changed -- same track request limiter
                        lastRequested.track = next.track;                                       // mark what we've requested last time
                        lastRequested.side  = next.side;

                        timeoutStart();                                                         // start a time-out timer

                        atnSendTrackRequest[4] = (((WORD)next.side) << 8) | (next.track);
                        spiDma_txRx(ATN_SENDTRACK_REQ_LEN_TX, (BYTE *) &atnSendTrackRequest[0], ATN_SENDTRACK_REQ_LEN_RX, (BYTE *) &readTrackData[0]);

                        outFlags.weAreReceivingTrack = TRUE;                                    // mark that we started to receive TRACK data
                    }
                }
            } else if(wrNow->readyToSend) {                                                     // not sending any ATN right now? and current write buffer has something?
                timeoutStart();                                                                 // start a time-out timer

                spiDma_txRx(wrNow->count, (BYTE *) &wrNow->buffer[0], 1, (BYTE *) &fakeBuffer);

                wrNow->readyToSend  = FALSE;                                                    // mark the current buffer as not ready to send (so we won't send this one again)

                wrNow               = wrNow->next;                                              // and now we will select the next buffer as current
                wrNow->readyToSend  = FALSE;                                                    // the next buffer is not ready to send (yet)
                wrNow->count        = 4;                                                        // at the start we already have 4 WORDs in buffer - SYNC, ATN code, TX len, RX len
            }
        }

        //-------------------------------------------------
        // if we got something in the cmd buffer, we should process it
        if(spiDmaIsIdle == TRUE && cmdBuffer[0] != CMD_MARK_READ_BYTE) {                        // if we're not sending (receiving) and the cmd buffer is not read
            int i;

            for(i=0; i<12; i++) {
                processHostCommand(cmdBuffer[i]);
            }

            cmdBuffer[0] = CMD_MARK_READ_BYTE;                                                  // mark that this cmd buffer is already read
        }

        //-------------------------------------------------
        inputs = GPIOB->IDR;                                        // read floppy inputs

        // now check if the drive is ON or OFF and handle it
        outFlags.stWantsTheStream = ((inputs & drive_select) == 0); // if motor is enabled and drive is selected, ST wants the stream

        //-------------------------------------------------

        WGate = inputs & WGATE;                                         // get current WGATE value

        if(WGate == 0) {                                                // when write gate is low, the data is written to floppy
            handleFloppyWrite();
            sectorsWritten++;                                       // one sector was written, request updated track at the end of stream
        }

        // fillMfmTimesForDMA -- execution time: 7 us - 16 us (16 us rarely, at the start / end)
        // times between two calls: 16 us - 53 us (16 us rarely, probably start / end of track)
        if((DMA1->ISR & (DMA1_IT_TC5 | DMA1_IT_HT5)) != 0) {        // MFM read stream: TC or HT interrupt? we've streamed half of circular buffer!
            fillMfmTimesForDMA();                                   // fill the circular DMA buffer with mfm times
        }

        //------------
        // check INDEX pulse as needed
        if((TIM2->SR & 0x0001) != 0) {          // overflow of TIM1 occured?
            TIM2->SR = 0xfffe;                  // clear UIF flag

            readTrackData_goToStart();          // move the pointer in the track stream to start

            //-----------
            if(sectorsWritten > 0) {        // if some sectors were written to floppy, we need to get the new stream now
                sectorsWritten = 0;         // nothing written now
                FORCE_REQUEST_TRACK;        // ask for the changed track data, but force it - get it immediatelly
            }

            //-----------
            // the following section of code should request track again if even after 2 rotations of floppy we're not streaming what we should
            trackStreamedCount++;               // increment the count of how many times we've streamed this track

            if(trackStreamedCount >= 2) {       // if since the last request 2 rotations happened
                if(streamed.track != now.track || streamed.side != now.side) {  // and we're not streaming what we really want to stream
                    REQUEST_TRACK;              // ask for track data (again?)
                }
            }
            streamed.track  = (BYTE) -1;        // after the end of track mark that we're not streaming anything
            streamed.side   = (BYTE) -1;
            //-----------

            fillReadStreamBufferWithDummyData();

            // the following few lines send the FW version to host every 5 index pulses, this is used for transfer of commands from host to Franz
            indexCount++;

            if(indexCount == 5) {
                indexCount = 0;
                sendFwVersion = TRUE;
            }
        }

        //--------
        // NOTE! Handling of STEP and SIDE only when MOTOR is ON, but the drive doesn't have to be selected and it must handle the control anyway
        if((inputs & MOTOR_ENABLE) != 0) {      // motor not enabled? Skip the following code.
            continue;
        }

        //------------
        // update SIDE var
        now.side = (inputs & SIDE1) ? 0 : 1;    // get the current SIDE
        if(prev.side != now.side) {             // side changed?
            REQUEST_TRACK;                      // we need track from the right side
            prev.side = now.side;
        }
    }
}

void fillReadStreamBufferWithDummyData(void)
{
    int i=0;

    for(i=0; i<16; i++) {               // copy 'all 4 us' pulses into current streaming buffer to allow shortest possible switch to start of track
        mfmReadStreamBuffer[i] = 7;
    }
}

void EXTI3_IRQHandler(void)
{
  if(EXTI_GetITStatus(EXTI_Line3) != RESET) {
        WORD inputs;
        WORD curIntTime, difIntTime;
        static WORD prevIntTime = 0;

        EXTI_ClearITPendingBit(EXTI_Line3);             // Clear the EXTI line pending bit
        inputs = GPIOB->IDR;

        //---------
        // now check if the step pulse isn't too soon after the previous one
        curIntTime = TIM4->CNT;                         // get current time -- 2 kHz timer
        difIntTime = curIntTime - prevIntTime;          // calc only difference

        if(difIntTime > 255) {
            difIntTime = 255;
        }

        if(difIntTime < 2) {                            // if the difference is less than 1 ms, quit
                return;
        }

        prevIntTime = curIntTime;                       // store as previous time
        //---------

        if((inputs & MOTOR_ENABLE) != 0) {              // motor not enabled? Skip the following code.
            return;
        }

        if(inputs & DIR) {                              // direction is High? track--
            if(now.track > 0) {
                now.track--;

                REQUEST_TRACK;
            }
        } else {                                        // direction is Low? track++
            if(now.track < 85) {
                now.track++;

                REQUEST_TRACK;
            }
        }

        if(now.track == 0) {                            // if track is 0
            GPIOB->BRR = TRACK0;                        // TRACK 0 signal to L
        } else {                                                    // if track is not 0
            GPIOB->BSRR = TRACK0;                       // TRACK 0 signal is H
        }
  }
}

void fillMfmTimesForDMA(void)
{
    WORD ind = 0;
    BYTE times4, time, i;

    // code to ARR value:      ??, 4us, 6us, 8us
    static WORD mfmTimes[4] = { 7,   7,  11,  15};

    // check for half transfer or transfer complete IF
    if((DMA1->ISR & DMA1_IT_TC5) != 0) {            // TCIF5 -- Transfer Complete IF 5
        ind = 8;
    }

    DMA1->IFCR = DMA1_IT_TC5 | DMA1_IT_HT5;         // clear HT5 and TC5 flag

    for(i=0; i<8; i++) {                            // convert all 4 codes to ARR values
        if(i==0 || i==4) {
            times4 = getNextMFMbyte();              // get next BYTE
        }

        time        = times4 >> 6;                  // get bits 15,14 (and then 13,12 ... 1,0)
        time        = mfmTimes[time];               // convert to ARR value
        times4  = times4 << 2;                      // shift 2 bits higher so we would get lower bits next time

        mfmReadStreamBuffer[ind] = time;            // store and move to next one
        ind++;
    }
}

void fillMfmTimesWithDummy(void)
{
    DMA1->IFCR = DMA1_IT_TC5 | DMA1_IT_HT5;         // clear HT5 and TC5 flag
    fillReadStreamBufferWithDummyData();
}

BYTE getNextMFMbyte(void)
{
    BYTE val;

    WORD maxLoops = READTRACKDATA_SIZE;

    while(1) {                                      // go through readTrackData to process commands and to find some data
        maxLoops--;

        if(maxLoops == 0) {                         // didn't quit the loop for 15k cycles? quit now!
            break;
        }

        readTrackData_get_noMove(val);              // get BYTE from buffer, but don't move

        if(val == CMD_TRACK_STREAM_END_BYTE) {      // we've hit the end of track stream? quit loop
            break;
        }

        readTrackData_justMove();                   // just move to the next position

        if(val == 0) {                              // skip empty WORDs
            continue;
        }

        // lower nibble == 0? it's a command from host - if we should turn on/off the write protect or disk change
        if(val == CMD_CURRENT_SECTOR) {             // it's a command?
            readTrackData_get(streamed.side);       // store side   #
            readTrackData_get(streamed.track);      // store track  #
            readTrackData_get(streamed.sector);     // store sector #
        } else {                                                        // not a command? return it
            return val;
        }
    }

    //---------
    // if we got here, we have no data to stream - just stream encoded zeros like in GAP 2 or GAP 3b
    return 0x55;
}

void findStreamTableOffset(void)
{
    int i;

    // load some default values
    streamTableOffset = 10;             // the stream table starts at this offset, because first 5 words are empty (ATN + sizes + other)
    streamStartOffset = streamTableOffset + STREAM_TABLE_SIZE;

    for(i=0; i<16; i++) {               // go through the read track data, find first non-zero value
        if(readTrackData[i]) {          // something non-zero at this index? this is the offset to stream table then
            streamTableOffset = i;
            streamStartOffset = streamTableOffset + STREAM_TABLE_SIZE;
            
            if(streamTableOffset != 10) {
                streamTableOffset = 10;
            }
            
            break;
        }
    }
}

void updateStreamPositionByFloppyPosition(void)
{
    DWORD mediaPosition;
    WORD *pStreamTable = (WORD *) (readTrackData + streamTableOffset);    // get pointer to stream table
    DWORD streamSize = pStreamTable[0];                      // get stream size (in bytes) from stream table at index 0

    // read the current position - from 0 to 400
    mediaPosition = TIM2->CNT;

    // calculate index where we should place sream reading index -
    // current position is between 0 and 400, that is from 0 to 100%, so place it between 0 and LENGTH OF STREAM position
    inIndexGet = (streamSize * mediaPosition) / 400;
}

void processHostCommand(BYTE val)
{
    switch(val) {
        case CMD_WRITE_PROTECT_OFF: isWriteProtected    = FALSE;    setupDiskChangeWriteProtect();  break;  // not write protected
        case CMD_WRITE_PROTECT_ON:  isWriteProtected    = TRUE;     setupDiskChangeWriteProtect();  break;  // is write protected
        case CMD_DISK_CHANGE_OFF:   isDiskChanged       = FALSE;    setupDiskChangeWriteProtect();                          break;  // not changed
        case CMD_DISK_CHANGE_ON:    isDiskChanged       = TRUE;     setupDiskChangeWriteProtect();  FORCE_REQUEST_TRACK;    break;  // has changed
        case CMD_GET_FW_VERSION:    sendFwVersion       = TRUE;                                     break;  // send FW version string and receive commands
        case CMD_SET_DRIVE_ID_0:    driveId             = 0;        setupDriveSelect();             break;  // set drive ID pins to check like this...
        case CMD_SET_DRIVE_ID_1:    driveId             = 1;        setupDriveSelect();             break;  // ...or that!
        case CMD_DRIVE_ENABLED:     driveEnabled        = TRUE;     setupDriveSelect();             break;  // drive is now enabled
        case CMD_DRIVE_DISABLED:    driveEnabled        = FALSE;    setupDriveSelect();             break;  // drive is now disabled
    }
}

void setupDriveSelect(void)
{
    if(driveEnabled == FALSE) {     // if drive not enabled, set drive_select mask to everything, so it will (probably) never find out that it's selected
        drive_select = 0xffff;
        return;
    }

    // if we got here, drive is enabled
    if(driveId == 0) {              // when drive ID is 0
        drive_select = MOTOR_ENABLE | DRIVE_SELECT0;
    } else {                        // when drive ID is 1
        drive_select = MOTOR_ENABLE | DRIVE_SELECT1;
    }
}

void setupDiskChangeWriteProtect(void)
{
    if(isDiskChanged) {                     // if disk HAS changed, write protect is inverted
        if(isWriteProtected) {
            GPIOB->BSRR     = WR_PROTECT;   // WR PROTECT to 1
        } else {
            GPIOB->BRR      = WR_PROTECT;   // WR PROTECT to 0
        }
        GPIOB->BRR          = DISK_CHANGE;  // DISK_CHANGE to 0
    } else {                                // if disk NOT changed, write protect behaves normally
        if(isWriteProtected) {
            GPIOB->BRR      = WR_PROTECT;   // WR PROTECT to 0
        } else {
            GPIOB->BSRR     = WR_PROTECT;   // WR PROTECT to 1
        }

        GPIOB->BSRR         = DISK_CHANGE;  // DISK_CHANGE to 1
    }
}

