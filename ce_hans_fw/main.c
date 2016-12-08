#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "misc.h"

#include "defs.h"
#include "timers.h"
#include "bridge.h"
#include "datatransfer.h"
#include "scsi.h"
#include "mmc.h"
#include "eeprom.h"

void init_hw_sw(void);
void onButtonPress(void);

void processHostCommands(void);
void handleAcsiConfig(BYTE acsiIds, BYTE sdId);
void handleFddConfig(BYTE enabledFddImages);
void handleFddSwitch(BYTE toWhichImage);

void spi_init1(void);
void spi_init2(void);
void dma_spi_init(void);
void dma_spi2_init(void);
void setupAtnBuffers(void);

WORD spi_TxRx(WORD out);
BYTE spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr);
BYTE spiDma_waitForFinish(void);        // returns if everything went well or timeout occured while waiting for that
BYTE waitForSPIidle(void);
void waitForSPI2idle(void);

BYTE onGetCommandAcsi(void);
BYTE onGetCommandScsi(void);
void getCmdLengthFromCmdBytesAcsi(void);
void getCmdLengthFromCmdBytesScsi(BYTE cmd);

void onGetCommand(void);
void onDataRead(void);
void onDataWrite(void);
void onReadStatus(void);

void startSpiDmaForDataRead(DWORD dataCnt, TReadBuffer *readBfr);
#define MIN(X,Y)        ((X < Y) ? (X) : (Y))

void showCurrentLED(void);
void fixLedsByEnabledImgs(void);

// cycle measure: t1 = TIM3->CNT;   t2 = TIM3->CNT; dt = t2 - t1; -- subtract 0x12 because that's how much measuring takes
WORD t1, t2, dt; 

// digital osciloscope time measure on GPIO B0:
//          GPIOB->BSRR = 1;    // 1
//          GPIOB->BRR = 1;     // 0            

// commands sent from device to host
#define ATN_FW_VERSION                      0x01                                // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND                    0x02
#define ATN_READ_MORE_DATA                  0x03
#define ATN_WRITE_MORE_DATA                 0x04
#define ATN_GET_STATUS                      0x05

// commands sent from host to device
#define CMD_ACSI_CONFIG                     0x10
#define CMD_DATA_WRITE                      0x20
#define CMD_DATA_READ                       0x30
#define CMD_SEND_STATUS                     0x40
#define CMD_FLOPPY_CONFIG                   0x70
#define CMD_FLOPPY_SWITCH                   0x80
#define CMD_DATA_MARKER                     0xda

// these states define if the device should get command or transfer data
#define STATE_GET_COMMAND                       0
#define STATE_SEND_COMMAND                      1
#define STATE_WAIT_COMMAND_RESPONSE             2
#define STATE_DATA_READ                         3
#define STATE_DATA_WRITE                        4
#define STATE_READ_STATUS                       5
#define STATE_SEND_FW_VER                       10


#define CMD_BUFFER_LENGTH                       16

///////////////////////////
// The following definitions are definitions of how many WORDs are TXed and RXed for each different ATN.
// Note that ATN_VARIABLE_LEN shouldn't be used, and is replaced by some value.

#define ATN_VARIABLE_LEN                0xffff

#define ATN_SENDFWVERSION_LEN_TX        10
#define ATN_SENDFWVERSION_LEN_RX        10

#define ATN_SENDACSICOMMAND_LEN_TX      12
#define ATN_SENDACSICOMMAND_LEN_RX      CMD_BUFFER_LENGTH

#define ATN_READMOREDATA_LEN_TX         6   
#define ATN_READMOREDATA_LEN_RX         ATN_VARIABLE_LEN

#define ATN_WRITEMOREDATA_LEN_TX        ATN_VARIABLE_LEN
#define ATN_WRITEMOREDATA_LEN_RX        1

#define ATN_GETSTATUS_LEN_TX            5
#define ATN_GETSTATUS_LEN_RX            8

///////////////////////////

#define ATN_SYNC_WORD                   0xcafe

BYTE state;
DWORD dataCnt;
BYTE statusByte;

WORD version[2] = {0xa016, 0x0426};                             // this means: hAns, 2016-04-26

char *VERSION_STRING_SHORT  = {"2.01"};
char *DATE_STRING           = {"04/26/16"};
                             // MM/DD/YY

volatile BYTE sendFwVersion;
WORD atnSendFwVersion[ATN_SENDFWVERSION_LEN_TX];
WORD atnSendACSIcommand[ATN_SENDACSICOMMAND_LEN_TX];

WORD seqNo = 0;
WORD atnMoreData[ATN_READMOREDATA_LEN_TX];

WORD atnGetStatus[ATN_GETSTATUS_LEN_TX];

TWriteBuffer    wrBuf1, wrBuf2;
TReadBuffer     rdBuf1, rdBuf2;
WORD smallDataBuffer[2];                

WORD cmdBuffer[CMD_BUFFER_LENGTH];

//----------
BYTE cmd[14];                                                   // received command bytes
BYTE cmdLen;                                                    // length of received command
BYTE brStat;                                                    // status from bridge

//----------
#ifdef CMDLOGGING
	#define CMDLOG_COUNT	256
	TCmdLogItem cmdLog[CMDLOG_COUNT];

	DWORD cmdSeqNo;
	DWORD cmdLogIndex;
	
	void memcpy(char *dest, char *src, int cnt);
	
	void cmdLog_onStart(void);
	void cmdLog_storeResults(BYTE bridgeStatus, BYTE scsiStatus);
	void cmdLog_onEnd(void);
	
#endif
//----------

BYTE enabledIDs[8];                                             // when 1, Hanz will react on that ACSI ID #

BYTE sdCardID;
BYTE prevIsCardInserted;
extern TDevice sdCard;

volatile BYTE spiDmaIsIdle;
volatile BYTE spiDmaTXidle, spiDmaRXidle;                       // flags set when the SPI DMA TX or RX is idle

BYTE currentLed;
BYTE enabledImgs[3];

BYTE firstConfigReceived;                                       // used to turn LEDs on after first config received

// TODO: check why the communication with host gets stuck when ST of off
// TODO: test and fix with megafile drive

// Note: DMA transfer to ODD address in ST looses first byte, so only transfer to EVEN address is OK.

BYTE shouldProcessCommands;

BYTE dataReadAsm(WORD *pData, WORD dataCnt);
DWORD isrNow, isrPrev;

WORD prevBtnPressTime;

WORD cardDetectTime;
BYTE cardInsertChanged;

void initAllStuff(void);

void getXilinxStatus(void);
void resetXilinx(void);
BYTE isBusIdle(void);
void handleAcsiCommand(void);

void serveButtonForRecoveryCmd(void);

BYTE xilinxHwFw;
BYTE hwVersion;
BYTE isAcsiNotScsi;
BYTE xilinxBusIdle;

#define MODE_UNKNOWN    0
#define MODE_WITH_RPI   1
#define MODE_SOLO       2

BYTE timeOutCount   = 0;
BYTE mode           = MODE_UNKNOWN;
void updateEnabledIDsInSoloMode(void);

BYTE tryProcessLocally(void);

BYTE lastErr;

DWORD toStart;
DWORD lastStart, lastEnd;

//--------------------------
#define LEDMODE_GOING_SOLO          1
#define LEDMODE_SHOWING_FDD_IMG     2
#define LEDMODE_STARTING_RECOVERY   3

struct {
    WORD goingSolo;
    WORD showingSelectedFddImage;
    WORD startingRecoveryCommand;
    
    BYTE currentMode;
    
    WORD nev;
    WORD prev;
} LEDs;

void updateLEDs(void);
//--------------------------

BYTE btnDownTime;
BYTE recoveryLevel_current;
BYTE recoveryLevel_toHost;

struct {
    WORD acsi;
    WORD fdd;
} configWords;

//--------------------------

int main(void)
{
    LOG_ERROR(0);           // no error
	
#ifdef CMDLOGGING	
		cmdSeqNo		= 0;
		cmdLogIndex	= 0;
#endif	
    
    initAllStuff();         // now init everything
    
    getXilinxStatus();
    resetXilinx();
    
    if(hwVersion != 2) {                    // for v.1 -- turn off MCO output
        GPIOA->CRH &= ~(0x0000000f);        // remove bits from GPIOA
        GPIOA->CRH |=   0x00000004;         // turn off MCO output on PA8 - leave it as floating input
    } else {                                // for v.2 -- turn on  MCO output
        GPIOA->CRH &= ~(0x0000000f);        // remove bits from GPIOA
        GPIOA->CRH |=   0x0000000b;         // turn on MCO output on PA8 - alternate function
    }
    
    //-------------
    // main loop
    while(1) {
        //---------------------------
        // set LEDs as needed
        
        updateLEDs();
        //---------------------------
        // START: SD card insertion / removal handling
        // if card insert state changed, mark down this event
        if(prevIsCardInserted != isCardInserted()) {
            prevIsCardInserted = isCardInserted();

            cardInsertChanged   = TRUE;
            cardDetectTime      = TIM1->CNT;
        }
        
        if(cardInsertChanged) {                                 // if card insert changed
            WORD cnt, diff;

            cnt     = TIM1->CNT;                                // get the current time
            diff    = cnt - cardDetectTime;

            if(diff >= 1000) {                                  // if it happened at least 0.5 s ago, do it
                cardInsertChanged = FALSE;
                sdCardInit();
            }
        }
        // END: SD card insertion / removal handling
        //---------------------------
        // get the command from ACSI and send it to host
        if(PIO_gotFirstCmdByte()) {                             // if 1st CMD byte was received
            handleAcsiCommand();
        }

        //---------------------------
        // START: SOLO / SLAVE mode checking and handling
        if(!spiDmaIsIdle && timeout()) {                                                        // if we got stuck somewhere, do IDLE again
            LOG_ERROR(1);
            
            spiDmaIsIdle = TRUE;
            state = STATE_GET_COMMAND;
            
            if(mode == MODE_UNKNOWN) {              // if we're in an uknown mode, move step further to SOLO mode, but not yet
                timeOutCount++;                     // increase count of how many times we must have timed out to keep working
    
                switch(timeOutCount) {
                    case 1: LEDs.goingSolo = LED1; break;
                    case 2: LEDs.goingSolo = LED2; break;
                    case 3: LEDs.goingSolo = LED3; break;
                }
            }
        }
        
        if(mode == MODE_UNKNOWN && timeOutCount >= 3) {                 // if we had to use timeout 3 times consequently to make this work, then we're probably in SOLO mode
            LEDs.goingSolo  = LED1 | LED2 | LED3;
            mode            = MODE_SOLO;
            state           = STATE_GET_COMMAND;
            sendFwVersion   = FALSE;
            
            sdCardID = EE_ReadVariable(EEPROM_OFFSET_SDCARD_ID, 0);     // get SD card ID, with default value of 0
            updateEnabledIDsInSoloMode();   // update enabled IDs
        }
        
        if(mode == MODE_SOLO) {             // solo mode? Skip the rest of the loop.
            continue;
        }
        
        // END: SOLO / SLAVE mode checking and handling
        //---------------------------
        // sending and receiving data over SPI using DMA
        if(spiDmaIsIdle && shouldProcessCommands) {                                             // SPI DMA: nothing to Tx and nothing to Rx?
            processHostCommands();                                                              // and process all the received commands
            
            shouldProcessCommands = FALSE;                                                      // mark that we don't need to process commands until next time
        }

        // in command waiting state, nothing to do and should send FW version?
        if(spiDmaIsIdle && sendFwVersion) {
            serveButtonForRecoveryCmd();                                                    // handle long btn press for issuing recovery cmd

            sendFwVersion = FALSE;
            
            timeoutStart();                                                                 // start timeout counter so we won't get stuck somewhere
            atnSendFwVersion[6] = (((WORD) currentLed) << 8) | xilinxHwFw;                  // store the current LED status in the last WORD, and also XILINX info byte value
            
            atnSendFwVersion[7] = (configWords.acsi        );
            atnSendFwVersion[8] = (configWords.fdd & 0xff00) | ((WORD) recoveryLevel_toHost);   // recovery code will be R, S or T
            recoveryLevel_toHost = 0;
            
            spiDma_txRx(    ATN_SENDFWVERSION_LEN_TX, (BYTE *) &atnSendFwVersion[0],     
                            ATN_SENDFWVERSION_LEN_RX, (BYTE *) &cmdBuffer[0]);
            
            shouldProcessCommands = TRUE;                                                   // mark that we should process the commands on next SPI DMA idle time
        }
        
        //---------------------------
        // if the button was pressed, handle it
        if(EXTI->PR & BUTTON) {
            onButtonPress();
        }
        
        //---------------------------
        // the following part is here to send FW version each second
        if((TIM2->SR & 0x0001) != 0) {                          // overflow of TIM2 occured?
            TIM2->SR = 0xfffe;                                  // clear UIF flag
            sendFwVersion = TRUE;
        }
    }
}

void updateLEDs(void)
{
    if(mode == MODE_WITH_RPI) {                         // when in RPI mode
        if(btnDownTime >= 2) {                          // when BTN is down, starting recovery
            LEDs.currentMode    = LEDMODE_STARTING_RECOVERY;
            LEDs.nev            = LEDs.startingRecoveryCommand;
        } else {                                        // when BTN not down, just showing FDD IMG
            LEDs.currentMode    = LEDMODE_SHOWING_FDD_IMG;
            LEDs.nev            = LEDs.showingSelectedFddImage;
        }        
    } else {                                            // when going solo
        LEDs.currentMode        = LEDMODE_GOING_SOLO;
        LEDs.nev                = LEDs.goingSolo;
    }

    if(LEDs.prev != LEDs.nev) {             // LEDs changed?
        LEDs.prev = LEDs.nev;
        
        GPIOA->BSRR = LED1 | LED2 | LED3;   // clear LEDs
        GPIOA->BRR  = LEDs.nev;             // set new LEDs
    }
}

void serveButtonForRecoveryCmd(void)
{
    WORD val;
    val = GPIOB->IDR;

    if(val & BUTTON) {          // button not pressed?
        if(btnDownTime >= 5) {  // the button was pressed long enough?
            if(btnDownTime >= 15) {
                recoveryLevel_current = 3;
            } else if(btnDownTime >= 10) {
                recoveryLevel_current = 2;
            } else if(btnDownTime >= 5) {
                recoveryLevel_current = 1;
            }
            
            LEDs.startingRecoveryCommand = 0;                       // turn off LEDs
            
            recoveryLevel_toHost    = recoveryLevel_current + 'Q';  // send this recovery level to host (R, S or T for level 1, 2 or 3)
            recoveryLevel_current   = 0;                            // set the current recovery level back to 0
        }
        
        btnDownTime = 0;
    } else {                    // button is pressed
        btnDownTime++;
        
        if(btnDownTime > 15) {  // presset too long? it's the longest press possible
            btnDownTime = 15;
        }

        if(btnDownTime >= 15) {
            LEDs.startingRecoveryCommand = LED1 | LED2 | LED3;
        } else if(btnDownTime >= 10) {
            LEDs.startingRecoveryCommand = LED1 | LED2;
        } else if(btnDownTime >= 5) {
            LEDs.startingRecoveryCommand = LED1;
        }
    }                
}

void handleAcsiCommand(void)
{
    // this is the loop which should go through the whole command processing (cmd phase, data phase, status phase) without other stuff
    while(1) {
        // get the command from ACSI and send it to host
        // IN  STATE: STATE_GET_COMMAND
        // OUT STATE: WAIT_COMMAND_RESPONSE when GOOD, STATE_GET_COMMAND when FAIL
        if(state == STATE_GET_COMMAND && PIO_gotFirstCmdByte()) {                   // if 1st CMD byte was received
            onGetCommand();
        }
        
        // transfer the data - read (to ST)
        // IN  STATE: STATE_DATA_READ
        // OUT STATE: always STATE_GET_COMMAND, but if everything is well, it also does PIO_read()
        if(state == STATE_DATA_READ) {
            timeoutStart();                         // start the timeout timer to give the rest of code full timeout time
            
            onDataRead();
            break;                                  // at this point it's either success or fail, but we're finished here
        }

        // transfer the data - write (from ST)
        // IN  STATE: STATE_DATA_WRITE
        // OUT STATE: STATE_READ_STATUS on success, STATE_GET_COMMAND on FAIL
        if(state == STATE_DATA_WRITE) {
            timeoutStart();                         // start the timeout timer to give the rest of code full timeout time
            
            onDataWrite();
        }
        
        // this happens after WRITE - wait for status byte, send it to ST (read)
        // IN  STATE: STATE_READ_STATUS
        // OUT STATE: always STATE_GET_COMMAND
        if(state == STATE_READ_STATUS) {
            timeoutStart();                         // start the timeout timer to give the rest of code full timeout time
            
            onReadStatus();
            break;                                  // at this point it's either success or fail, but we're finished here
        }

        // sending and receiving data over SPI using DMA
        // IN  STATE: any
        // OUT STATE: STATE_DATA_WRITE, STATE_DATA_READ, or unchanged
        if(spiDmaIsIdle && shouldProcessCommands) {                                             // SPI DMA: nothing to Tx and nothing to Rx?
            processHostCommands();                                                              // and process all the received commands
            
            shouldProcessCommands = FALSE;                                                      // mark that we don't need to process commands until next time
        }
        
        if(timeout()) {         // if the data from host doesn't come within timeout, quit
            LOG_ERROR(50);
            state = STATE_GET_COMMAND;
            break;
        }
        
        // if we came here and we are in the basic state, go to the outside loop to do the rest of the code
        if(state == STATE_GET_COMMAND) {
            break;
        }
    }

    //---------------
    // if something was wrong, reset XILINX so it won't get stuck 
    if(brStat != E_OK) {
        resetXilinx();
    }
    
    // The following goes only for SCSI interface, because current getXilinxStatus() (which is called from isBusIdle())
    // triggers INT going low, and thus blocks FDD. The issue is somewhere in the Xilinx code or in the idea to use 
    // both XPIO & XDMA going high for this getXilinxStatus(). 
    if(!isAcsiNotScsi) {        // only for SCSI interface!
        if(!isBusIdle()) {      // if the bus is not idle, do the reset
            resetXilinx();
        }
    }
}

void initAllStuff(void)
{
    BYTE i;
    
    state = STATE_GET_COMMAND;
    
    sendFwVersion       = FALSE;
    firstConfigReceived = FALSE;                                    // used to turn LEDs on after first config received

    setupAtnBuffers();                                              // fill the ATN buffers with needed headers and terminators

    spiDmaIsIdle = TRUE;
    spiDmaTXidle = TRUE;
    spiDmaRXidle = TRUE;
    
    currentLed = 0xff;                                              // current LED - no LED

    shouldProcessCommands = FALSE;

    init_hw_sw();                                                   // init GPIO pins, timers, DMA, global variables

    // init ACSI signals
    ACSI_DATADIR_WRITE();                                           // data as inputs
    GPIOA->BRR = aDMA | aPIO | aRNW | ATN;                          // ACSI controll signals LOW, ATTENTION bit LOW - nothing to read

    // init SD card interface signals
    GPIOC->BRR  = XMSG;
    GPIOC->BSRR = SD_DETECT | SD_CS;                                // SD card CS (output) high - card not selected; SD card detect high - pull up!

    EXTI->PR = BUTTON | aCMD | aCS | aACK;                          // clear these ints 
    
    //-------------
    // by default disable all ACSI IDs 
    for(i=0; i<8; i++) {
        enabledIDs[i] = FALSE;
    }
    
    sdCardID = 0xff;                                                // for SD card set non-existing ID for now
    
    // now for floppy image LEDs - enable IMG0 (even if nothing is there), disable other
    for(i=1; i<3; i++) {
        enabledImgs[i] = FALSE;
    }
    
    prevBtnPressTime = 0;

    //----------------------
    // init card detection delay stuff
    cardInsertChanged   = FALSE;
    cardDetectTime      = 0;
    
    // now init the card if it's inserted
    prevIsCardInserted = isCardInserted();
    if(prevIsCardInserted) {
        sdCardInit();
        
        sdCard.MediaChanged = FALSE;                    // after first init the SD card is not not in MEDIA CHANGED state
    } else {
        sdCardZeroInitStruct();
    }
    
//    scsi_log_init();
}

void onButtonPress(void)
{
    WORD cnt, diff;

    EXTI->PR = BUTTON;                                  // clear pending EXTI
    
    cnt = TIM1->CNT;                                    // get the current time
    diff = cnt - prevBtnPressTime;

    if(diff < 500) {                                    // the previous button press happened less than 250 ms before? ignore
        return;
    }   
    
    prevBtnPressTime = cnt;                             // store current time
    
    currentLed++;                                       // change LED
    
    if(currentLed > 2) {                                // overflow?
        currentLed = 0xff;
    }
    
    fixLedsByEnabledImgs();                             // if we switched to not enabled floppy image, fix this
    
    showCurrentLED();                                   // and show the current LED
}

void fixLedsByEnabledImgs(void)
{
    BYTE i;
    
    for(i=0; i<3; i++) {                                    // try to switch to enabled LED 3 times
        if(currentLed == 0xff) {                            // no LED? quit
            return;
        }
    
        if(enabledImgs[currentLed] == TRUE) {               // if this LED is enabled, use it
            return;
        }

        // current LED is not enabled, switch to another one
        currentLed++;                                       // change LED
    
        if(currentLed > 2) {                                // overflow? fix it
            currentLed = 0xff;
        }
    }

    currentLed = 0xff;                                      // use NO LED as selected                                          
}

void showCurrentLED(void)
{
    switch(currentLed) {                                    // turn on the correct LED
        case 0:     LEDs.showingSelectedFddImage = LED1;    break;
        case 1:     LEDs.showingSelectedFddImage = LED2;    break;
        case 2:     LEDs.showingSelectedFddImage = LED3;    break;
        default:    LEDs.showingSelectedFddImage = 0;       break;
    }
}

void onGetCommand(void)
{
    BYTE i, id;
    
    //---------
    // retrieve the command. There are some slight differences between ACSI and SCSI part, 
    // but the resulting commands should be the same (to make the rest of app work without further changes).
    BYTE good;
    
    if(isAcsiNotScsi) {                         // for ACSI
        good = onGetCommandAcsi();
    } else {                                    // for SCSI
        good = onGetCommandScsi();
    }

#ifdef CMDLOGGING
		cmdLog_onStart();
#endif
		
    if(!good) {                                 // if failed to get the cmd, quit
#ifdef CMDLOGGING
				cmdLog_storeResults(brStat, 0xff);
				cmdLog_onEnd();
#endif
        return;
    }
    
    id = (cmd[0] >> 5) & 0x07;                  // get only device ID
    //-----
    // if we came here, everything went OK
    if(id == sdCardID) {                        // for SD card IDs
        BYTE processedLocally;
        processedLocally = tryProcessLocally(); // try to process command locally
        
        if(processedLocally) {                  // if it was processed locally, quit
            return;
        }
    }

    if(mode == MODE_SOLO) {                     // if in solo mode, don't send ACSI command to host
        return;
    }
    
    //----------------
    // if we got here, we should handle this in host (RPi)
    for(i=0; i<7; i++) {                // fill the command to array
        atnSendACSIcommand[4 + i] = (((WORD)cmd[i*2 + 0]) << 8) | cmd[i*2 + 1];
    }

    timeoutStart();                     // start the timeout timer to give the rest of code full timeout time
    
    spiDma_txRx(    ATN_SENDACSICOMMAND_LEN_TX, (BYTE *) &atnSendACSIcommand[0], 
                    ATN_SENDACSICOMMAND_LEN_RX, (BYTE *) &cmdBuffer[0]);
    
    state = STATE_WAIT_COMMAND_RESPONSE;
    shouldProcessCommands = TRUE;       // mark that we should process the commands on next SPI DMA idle time
}

BYTE onGetCommandAcsi(void)
{
    BYTE id, i;

    //----------------------
    cmd[0]  = PIO_writeFirst();                     // get byte from ST (waiting for the 1st byte)
    id      = (cmd[0] >> 5) & 0x07;                 // get only device ID

    //----------------------
    if(!enabledIDs[id]) {                           // if this ID is not enabled, quit
        return 0;
    }
    
    cmdLen = 6;                                     // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()
            
    for(i=1; i<cmdLen; i++) {                       // receive the next command bytes
        cmd[i] = PIO_write();                       // drop down IRQ, get byte

        if(brStat != E_OK) {                        // if something was wrong, quit, failed
            resetXilinx();
            return 0;
        }
        
        if(i == 1) {                                // if we got also the 2nd byte
            getCmdLengthFromCmdBytesAcsi();         // we set up the length of command, etc.
        }             
    }
    
    return 1;
}

BYTE onGetCommandScsi(void)
{
    BYTE id;
    BYTE sel;
    int i;

    //----------------------
    sel     = PIO_writeFirst();                 // get SELection byte
    id      = 0xff;                             // mark that ID hasn't been found yet
    
    for(i=0; i<8; i++) {
        if((sel & (1 << i)) != 0) {             // if bit is one, this ID is selected 
            if(enabledIDs[i]) {                 // if that ID is enabled
                id = i;                         // store this ID and quit loop
                break;
            }
        }
    }
    
    if(id == 0xff) {                            // ID not found? quit
        return 0;
    }
    //----------------------
    if(!enabledIDs[id]) {                           // if this ID is not enabled, quit
        return 0;
    }
    
    cmdLen = 6;                                     // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()
            
    for(i=0; i<cmdLen; i++) {                       // receive the next command bytes
        cmd[i] = PIO_write();                       // drop down IRQ, get byte

        if(brStat != E_OK) {                        // if something was wrong, quit, failed
            resetXilinx();
            return 0;
        }
        
        if(i == 0) {                                // if we got also the 2nd byte
            getCmdLengthFromCmdBytesScsi(cmd[0]);   // we set up the length of command, etc.
        }             
    }
    
    // now fix the command if the length is more than 6 bytes
    if(cmdLen > 6) {
        for(i=13; i>0; i--) {                       // move the cmd one byte further (to make cmd[0] unused)
            cmd[i] = cmd[i - 1];
        }
        cmd[0] = 0x1f;                              // store ICD command marker
        
        cmdLen++;                                   // now the command is one byte longer
    }
    
    // for all commands add fake ACSI ID on top of the 0th byte
    cmd[0] = cmd[0] | (id << 5);                    // add ID on the top 3 bits
    return 1;
}

BYTE tryProcessLocally(void)
{
    BYTE justCmd;
    BYTE tag1, tag2, isIcd;
    BYTE cmdIsForCE = FALSE;
    
    //----------------------
    // figure out it this has CosmosEx custom command format
    justCmd = cmd[0] & 0x1f;                // get just the command (remove ACSI ID)

    if(justCmd == 0x1f) {                   // if it's ICD command, get the next byte as command
        justCmd = cmd[1];                   // get just the command
        isIcd   = TRUE;
        
        if(justCmd == 0xa0) {               // if it's REPORT LUNS command, it might be for CE
            cmdIsForCE = TRUE;                        
        }
        
        tag1    = cmd[2];                   // CE tag ('C', 'E') can be found on position 2 and 3
        tag2    = cmd[3];
    } else {                                // if it's not ICD command
        isIcd   = FALSE;

        if(justCmd == 0) {                  // if just command (without ACSI ID) is TEST UNIT READY, it's for CE
            cmdIsForCE = TRUE;
        }
        
        tag1    = cmd[1];                   // CE tag ('C', 'E') can be found on position 1 and 2
        tag2    = cmd[2];
    }

    if(firstConfigReceived == FALSE) {      // no config received from host? Then process it localy.
        cmdIsForCE = FALSE;
    }

    if(cmdIsForCE == TRUE && tag1 == 'C' && tag2 == 'E') {  // this is a Cosmos Ex specific command? Don't process locally!
        return FALSE;                                       // didn't process local
    }
    
    //---------
    // if it's not CosmosEx custom command, process it locally
    processScsiLocaly(justCmd, isIcd);
    
//  scsi_log_add();
    return TRUE;                        // did process locally
}

// when in SOLO mode, eable and disable IDs according to sdCardID
void updateEnabledIDsInSoloMode(void)
{
    BYTE i;
    
    if(mode != MODE_SOLO) {             // not solo mode? quit
        return;
    }
    
    for(i=0; i<8; i++) {        // enable ID for SD card, disable others
        if(i == sdCardID) {
            enabledIDs[i] = 1;
        } else {
            enabledIDs[i] = 0;
        }
   }
}

void onDataRead(void)
{
    WORD i, loopCount, l, dataBytesCount;
    BYTE dataMarkerFound;
    BYTE res;
    TReadBuffer *rdBufNow;
    WORD *pData;

    seqNo = 0;
    state = STATE_GET_COMMAND;                                                          // this will be the next state once this function finishes
    
    // nothing to send? then just quit with status byte
    if(dataCnt == 0) {                                                                  
        PIO_read(statusByte);
        return;
    }
    
    // calculate how many loops we will have to do 
    loopCount = dataCnt / 512;

    if((dataCnt % 512) != 0) {
        loopCount ++;
    }
    
    // receive 0th data block in rdBuf1
    rdBufNow = &rdBuf1;

    startSpiDmaForDataRead(dataCnt, rdBufNow);
    dataCnt -= (DWORD) rdBufNow->dataBytesCount;                                        // update remaining data size
    
    // now start the double buffered transfer to ST
    ACSI_DATADIR_READ();                                                                // data direction for reading
    
    for(l=0; l<loopCount; l++) {
        // first wait until all data arrives in SPI DMA transfer
        while(!spiDmaIsIdle) {
            if(timeout()) {                                                             // if the data from host doesn't come within timeout, quit
                LOG_ERROR(2);
                spiDmaIsIdle = TRUE;
                ACSI_DATADIR_WRITE();                                                   // data direction for writing, and quit
                return;
            }
        }

        // if after transfering this block there should be another block of data
        if(dataCnt > 0) {                           
            TReadBuffer *nextRdBuffer = rdBufNow->next;
            
            startSpiDmaForDataRead(dataCnt, nextRdBuffer);                              // start receiving data to the other buffer
            dataCnt -= (DWORD) nextRdBuffer->dataBytesCount;                            // update remaining data size
        }
    
        ///////////////////////////////////////////////////////////////
        // send the received data to ST
        // find the data marker
        dataMarkerFound = FALSE;
        pData = &rdBufNow->buffer[0];
        
        for(i=0; i<rdBufNow->count; i++) {
            WORD data;
            
            data = *pData;                                  // get data
            pData++;

            if(data == CMD_DATA_MARKER) {                   // found data marker?
                dataMarkerFound = TRUE;
                break;
            }
        }
        
        if(dataMarkerFound == FALSE) {                      // didn't find the data marker?
            PIO_read(0x02);                                 // send status: CHECK CONDITION and quit
            return;
        }

        // now try to trasmit the data
        dataBytesCount = rdBufNow->dataBytesCount;

        res = dataReadCloop(pData, dataBytesCount);
    
        if(res == 0) {
            ACSI_DATADIR_WRITE();                           // data direction for writing, and quit
            return;
        }
        
        // one cycle finished, now swap buffers and start all over again
        rdBufNow = rdBufNow->next;                          // swap buffers
    }
    
    PIO_read(statusByte);                                   // send the status to Atari
}

BYTE waitForSPIidle(void)
{
    while((SPI1->SR & SPI_SR_TXE) == 0) {                   // wait while TXE flag is 0 (TX is not empty)
        if(timeout()) {
            LOG_ERROR(3);
            return FALSE;                                   // exit - timeout!
        }
    }

    while((SPI1->SR & SPI_SR_BSY) != 0) {                   // wait while BSY flag is 1 (SPI is busy)
        if(timeout()) {
            LOG_ERROR(4);
            return FALSE;                                   // exit - timeout!
        }
    }
    
    return TRUE;                                            // exit - success (no timeout)
}

void waitForSPI2idle(void)
{
    while((SPI2->SR & SPI_SR_TXE) == 0) {                   // wait while TXE flag is 0 (TX is not empty)
        if(timeout()) {
            LOG_ERROR(5);
            return;
        }
    }

    while((SPI2->SR & SPI_SR_BSY) != 0) {                   // wait while BSY flag is 1 (SPI is busy)
        if(timeout()) {
            LOG_ERROR(6);
            return;
        }
    }
}

void startSpiDmaForDataRead(DWORD dataCnt, TReadBuffer *readBfr)
{
    DWORD subCount, recvCount;

    // calculate how much we need to transfer in 0th data buffer
    subCount     = MIN(dataCnt, 512);                       // BYTEs to receive
    recvCount    = subCount / 2;                            // WORDs to receive: convert # of BYTEs to # of WORDs
    recvCount   += (subCount & 1);                          // if subCount is odd number, then we need to transfer 1 WORD more
    recvCount   += 6;                                       // receive few words more than just data - 2 WORDs start, 1 WORD CMD_DATA_MARKER

    readBfr->dataBytesCount = subCount;                     // store the count of ONLY data
    readBfr->count          = recvCount;                    // store the count of ALL WORDs in buffer, including headers and other markers
    
    // now transfer the 0th data buffer over SPI
    atnMoreData[4] = seqNo++;                                                               // set the sequence # to Attention
    spiDma_txRx(    6,  (BYTE *) &atnMoreData[0], 
                        recvCount,  (BYTE *) &readBfr->buffer[0]);                          // set up the SPI DMA transfer
}

void onDataWrite(void)
{
    BYTE firstLoop, previousSpiSuccess;
    WORD subCount, recvCount;
    WORD index, data, i, value;
    TWriteBuffer *wrBufNow;
    
    seqNo           = 0;
    wrBufNow    = &wrBuf1;
    
    ACSI_DATADIR_WRITE();                                   // data direction for reading
    
    firstLoop = TRUE;
    
    while(dataCnt > 0) {            // something to write?
        // request maximum 512 bytes from host 
        subCount = (dataCnt > 512) ? 512 : dataCnt;
        dataCnt -= subCount;
                
        wrBufNow->buffer[4] = seqNo;                        // set the sequence # to Attention
        seqNo++;
        
        recvCount   = subCount / 2;                         // WORDs to receive: convert # of BYTEs to # of WORDs
        recvCount   += (subCount & 1);                      // if subCount is odd number, then we need to transfer 1 WORD more
        
        index = 5;                                          // length of header before data
        
        for(i=0; i<recvCount; i++) {                        // write this many WORDs
            value = DMA_write();                            // get data from Atari
            value = value << 8;                             // store as upper byte
                
            if(brStat == E_TimeOut) {                       // if timeout occured
                state = STATE_GET_COMMAND;                  // transfer failed, don't send status, just get next command
                return;
            }

            subCount--;
            if(subCount == 0) {                             // in case of odd data count
                wrBufNow->buffer[index] = value;            // store data
                index++;
                
                break;
            }

            data = DMA_write();                             // get data from Atari
            value = value | data;                           // store as lower byte
                
            if(brStat == E_TimeOut) {                       // if timeout occured
                state = STATE_GET_COMMAND;                  // transfer failed, don't send status, just get next command
                return;
            }

            subCount--;
                            
            wrBufNow->buffer[index] = value;                // store data
            index++;
        }
        
        wrBufNow->buffer[index] = 0;                        // terminating zero
        wrBufNow->count = index + 1;                        // store count, +1 because we have terminating zero

        //----------
        // set up the SPI DMA transfer
        previousSpiSuccess = spiDma_txRx(wrBufNow->count,                        (BYTE *) &wrBufNow->buffer[0], 
                                                     ATN_WRITEMOREDATA_LEN_RX,   (BYTE *) &smallDataBuffer[0]);

        // if this is not the first loop and the previous SPI transfer failed (something from this WRITE command was not transfered to host), fail
        if(!firstLoop && !previousSpiSuccess) {
            state = STATE_GET_COMMAND;
            return;
        }
        
        firstLoop = FALSE;
        //----------
        
        wrBufNow = wrBufNow->next;                          // use next write buffer
    }

    state = STATE_READ_STATUS;                              // continue with sending the status
}

void onReadStatus(void)
{
    BYTE i, newStatus;
    
    newStatus = 0xff;                                       // no status received
    
    spiDma_txRx(    ATN_GETSTATUS_LEN_TX, (BYTE *) &atnGetStatus[0], 
                                ATN_GETSTATUS_LEN_RX, (BYTE *) &cmdBuffer[0]);

    spiDma_waitForFinish();

    for(i=0; i<8; i++) {                                    // go through the received buffer
        if(cmdBuffer[i] == CMD_SEND_STATUS) {
            newStatus = cmdBuffer[i+1] >> 8;
            break;
        }
    }

    PIO_read(newStatus);                                    // send the status to Atari
    state = STATE_GET_COMMAND;                              // get the next command
}

void getCmdLengthFromCmdBytesAcsi(void)
{   
    // now it's time to set up the receiver buffer and length
    if((cmd[0] & 0x1f)==0x1f)   {                           // if the command is '0x1f'
        switch((cmd[1] & 0xe0)>>5)                          // get the length of the command
        {
            case  0: cmdLen =  7; break;
            case  1: cmdLen = 11; break;
            case  2: cmdLen = 11; break;
            case  5: cmdLen = 13; break;
            default: cmdLen =  7; break;
        }
    } else {                                                // if it isn't a ICD command
        cmdLen   = 6;                                       // then length is 6 bytes 
    }
}

void getCmdLengthFromCmdBytesScsi(BYTE cmd)
{   
    switch((cmd & 0xe0)>>5)                                 // get the length of the command
    {
        case  0: cmdLen =  6; break;
        case  1: cmdLen = 10; break;
        case  2: cmdLen = 10; break;
        case  4: cmdLen = 16; break;
        case  5: cmdLen = 12; break;
        default: cmdLen =  6; break;
    }
}

BYTE spiDma_waitForFinish(void)
{
    while(spiDmaIsIdle != TRUE) {                           // wait until it will become idle
        if(timeout()) {                                     // if timeout happened (and we got stuck here), quit
            LOG_ERROR(7);
            spiDmaIsIdle = TRUE;
            return FALSE;               // exit - timeout fail!
        }
    }
    
    return TRUE;                        // exit - everything OK (no timeout)
}

BYTE spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr)
{
    BYTE waitForFinishOk, waitForSpiIdleOk;
    
    WORD *pTxBfr = (WORD *) txBfr;
    
    // store TX and RX count so the host will know how much he should transfer
    pTxBfr[2] = txCount;
    pTxBfr[3] = rxCount;
    
    waitForFinishOk     = spiDma_waitForFinish();           // make sure that the last DMA transfer has finished
    waitForSpiIdleOk    = waitForSPIidle();                 // and make sure that SPI has finished, too
    
    // disable both TX and RX channels
    DMA1_Channel3->CCR      &= 0xfffffffe;                  // disable DMA3 Channel transfer
    DMA1_Channel2->CCR      &= 0xfffffffe;                  // disable DMA2 Channel transfer

    //-------------------
    // The next simple 'if' is here to help the last word of block loss (first word of block not present),
    // it doesn't do much (just gets a byte from RX register if there is one waiting), but it helps the situation -
    // without it the problem occures, with it it seems to be gone (can't reproduce). This might be caused just
    // by the adding the delay between disabling and enabling DMA by this extra code. 
    
    if((SPI1->SR & SPI_SR_RXNE) != 0) {                     // if there's something still in SPI DR, read it
        WORD dummy = SPI1->DR;
    }
    //-------------------
    
    // set the software flags of SPI DMA being idle
    spiDmaTXidle = (txCount == 0) ? TRUE : FALSE;           // if nothing to send, then IDLE; if something to send, then FALSE
    spiDmaRXidle = (rxCount == 0) ? TRUE : FALSE;           // if nothing to receive, then IDLE; if something to receive, then FALSE
    spiDmaIsIdle = FALSE;                                   // SPI DMA is busy
    
    // config SPI1_TX -- DMA1_CH3
    DMA1_Channel3->CMAR     = (uint32_t) txBfr;             // from this buffer located in memory
    DMA1_Channel3->CNDTR    = txCount;                      // this much data
    
    // config SPI1_RX -- DMA1_CH2
    DMA1_Channel2->CMAR     = (uint32_t) rxBfr;             // to this buffer located in memory
    DMA1_Channel2->CNDTR    = rxCount;                      // this much data

    // enable both TX and RX channels
    DMA1_Channel3->CCR      |= 1;                           // enable  DMA1 Channel transfer
    DMA1_Channel2->CCR      |= 1;                           // enable  DMA1 Channel transfer
    
    // now set the ATN pin accordingly
    if(txCount != 0) {                                      // something to send over SPI?
        GPIOA->BSRR = ATN;                                  // ATTENTION bit high - got something to read
    }
    
    if(waitForFinishOk && waitForSpiIdleOk) {               // both SPI waiting functions without error? Then this SPI transfer was stared without any previous error
        return TRUE;
    }
    
    return FALSE;                                           // some of the SPI idle waiting functions failed, previous transfer probably failed
}

void spi2Dma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr)
{
    waitForSPI2idle();                                       // and make sure that SPI has finished, too
    
    // disable both TX and RX channels
    DMA1_Channel5->CCR      &= 0xfffffffe;                  // disable DMA5 Channel transfer
    DMA1_Channel4->CCR      &= 0xfffffffe;                  // disable DMA4 Channel transfer

    //-------------------
    // The next simple 'if' is here to help the last word of block loss (first word of block not present),
    // it doesn't do much (just gets a byte from RX register if there is one waiting), but it helps the situation -
    // without it the problem occures, with it it seems to be gone (can't reproduce). This might be caused just
    // by the adding the delay between disabling and enabling DMA by this extra code. 
    
    if((SPI2->SR & SPI_SR_RXNE) != 0) {                     // if there's something still in SPI DR, read it
        WORD dummy = SPI2->DR;
    }
    //-------------------
    // config SPI2_TX -- DMA1_CH5
    DMA1_Channel5->CMAR     = (uint32_t) txBfr;             // from this buffer located in memory
    DMA1_Channel5->CNDTR    = txCount;                      // this much data
    
    // config SPI2_RX -- DMA1_CH4
    DMA1_Channel4->CMAR     = (uint32_t) rxBfr;             // to this buffer located in memory
    DMA1_Channel4->CNDTR    = rxCount;                      // this much data

    // enable both TX and RX channels
    DMA1_Channel4->CCR      |= 1;                           // enable  DMA1 Channel transfer
    DMA1_Channel5->CCR      |= 1;                           // enable  DMA1 Channel transfer
}

void init_hw_sw(void)
{
    DWORD a;
    
    RCC->AHBENR     |= (1 <<  0);                                               // enable DMA1
    RCC->APB1ENR    |= (1 << 14) | (1 <<  2) | (1 << 1) | (1 << 0);             // enable SPI2, TIM4, TIM3, TIM2
    RCC->APB2ENR    |= (1 << 12) | (1 << 11) | (1 << 4) | (1 << 3) | (1 << 2);  // Enable SPI1, TIM1, GPIOC, GPIOB, GPIOA clock

    // SPI1 -- enable atlernate function for PA4, PA5, PA6, PA7
    GPIOA->CRL &= ~(0xffff0000);                            // remove bits from GPIOA
    GPIOA->CRL |=   0xbbbb0000;                             // set GPIOA as --- CNF1:0 -- 10 (push-pull), MODE1:0 -- 11, PxODR -- don't care

    // ACSI input signals as floating inputs
    GPIOB->CRH &= ~(0x000fffff);                            // clear 5 lowest bits
    GPIOB->CRH |=   0x00044444;                             // set as 5 inputs (BUTTON, ACK, CS, CMD, RESET)

    // SPI2 -- enable atlernate function for PB13, PB14, PB15
    GPIOB->CRH &= ~(0xfff00000);                            // remove bits from GPIOB
    GPIOB->CRH |=   0xbbb00000;                             // set GPIOB as --- CNF1:0 -- 10 (push-pull), MODE1:0 -- 11, PxODR -- don't care

    // card detect and SPI2 CS on GPIOC -- CS on PC13 is output, card detect on PC14 as input with pull up! XMSG as OUTPUT
    GPIOC->CRH &= ~(0xfff00000);                            // remove bits from GPIOC
    GPIOC->CRH |=   0x38300000;

    // ACSI control signals as output, +ATN
    GPIOA->CRL &= ~(0x0000ffff);                            // remove bits from GPIOA
    GPIOA->CRL |=   0x00003333;                             // 4 ouputs (ATN, DMA, PIO, RNW) and 4 inputs (ACK, CS, CMD, RESET)

    // LEDs
    GPIOA->CRH &= ~(0xf00ff00f);                            // remove bits from GPIOA
    GPIOA->CRH |=   0x3003300b;                             // 3 ouputs - LED1, LED2, LED3, PA8 as ALT output (MCO)

    RCC->APB2ENR |= (1 << 0);                               // enable AFIO
    
    AFIO->EXTICR[2] = 0x1110;                               // source input: GPIOB for EXTI 9, 10, 11
    AFIO->EXTICR[3] = 0x0001;                               // source input: GPIOB for EXTI 12 -- button
    EXTI->IMR       = BUTTON | aCMD | aCS | aACK;           // 1 means: Interrupt from these lines is not masked
    EXTI->EMR       = BUTTON | aCMD | aCS | aACK;           // 1 means: Event     form these lines is not masked
    EXTI->FTSR      = BUTTON | aCMD | aCS | aACK;           // Falling trigger selection register

    GPIOA->BSRR = LED1 | LED2 | LED3;                       // all LED pins high (LEDs OFF)
    //----------
    AFIO->MAPR |= 0x02000000;                               // SWJ_CFG[2:0] (Bits 26:24) -- 010: JTAG-DP Disabled and SW-DP Enabled
    
    timerSetup_buttonTimer();                               // TIM1 counts at 2 kHz as a button timer (to not allow many buttons presses one after another)
    timerSetup_sendFw();                                    // set TIM2 to 1 second to send FW version each second
    timerSetup_cmdTimeout();                                // set TIM3 to 1 second as a timeout for command processing
    
    //--------------
    // DMA + SPI initialization
    
    spi_init1();                                            // init SPI interface - for RPi
    dma_spi_init();

    spi_init2();                                            // init SPI interface - for SD card
    dma_spi2_init();
    
	// enable clock output on PA8 pin
    a = RCC->CFGR;
    a = a & 0xf8ffffff;         // remove MCO bits
    a = a | 0x07000000;         // MCO (micro clock output): PLL/2 (36 MHz)
//  a = a | 0x06000000;         // MCO (micro clock output): ext (8 MHz)
    RCC->CFGR = a;
}

WORD spi_TxRx(WORD out)
{
    WORD in;

    while((SPI1->SR & (1 << 7)) != 0);                      // TXE flag: BUSY flag

    while((SPI1->SR & 2) == 0);                             // TXE flag: Tx buffer empty
    SPI1->DR = out;                                         // send over SPI

    while((SPI1->SR & 1) == 0);                             // RXNE flag: RX buffer NOT empty
    in = SPI1->DR;                                          // get data
    
    return in;
}

void processHostCommands(void)
{
    BYTE i, j;
    
    for(i=0; i<CMD_BUFFER_LENGTH; i++) {
        switch(cmdBuffer[i]) {
            // process ACSI configuration 
            case CMD_ACSI_CONFIG:
                handleAcsiConfig((BYTE) (cmdBuffer[i+1] >> 8), (BYTE) cmdBuffer[i+1]);
                handleFddConfig ((BYTE) (cmdBuffer[i+2] >> 8));
        
                configWords.acsi    = cmdBuffer[i+1];                   // store config words
                configWords.fdd     = cmdBuffer[i+2] & 0xff00;
            
                cmdBuffer[i]    = 0;                                    // clear this command
                cmdBuffer[i+1]  = 0;
                cmdBuffer[i+2]  = 0;
                
                i += 2;
                break;
            
            // this command is used so the host may select the a different LED light of selected image
            case CMD_FLOPPY_SWITCH:
                handleFddConfig((BYTE) (cmdBuffer[i+1] >> 8));      // upper byte - enabled images
                handleFddSwitch((BYTE) (cmdBuffer[i+1]     ));      // lower byte - selected image

                configWords.fdd = cmdBuffer[i+1] & 0xff00;          // store this config word
            
                cmdBuffer[i]    = 0;                                // clear this command
                cmdBuffer[i+1]  = 0;
                    
                i++;
                break;
                
            case CMD_DATA_WRITE:
                    dataCnt             = cmdBuffer[i+1];           // store the count of bytes we should WRITE - highest and middle byte
                    dataCnt             = dataCnt << 8;
                    dataCnt             |= cmdBuffer[i+2] >> 8;     // lowest byte
            
                    statusByte      = cmdBuffer[i+2] & 0xff;
                    
                    for(j=0; j<3; j++) {                            // clear this command 
                        cmdBuffer[i + j] = 0;               
                    }
                    
                    state = STATE_DATA_WRITE;                       // go into DATA_WRITE state
                    i += 2;
                break;
                
            case CMD_DATA_READ:     
                    dataCnt             = cmdBuffer[i+1];           // store the count of bytes we should READ - highest and middle byte
                    dataCnt             = dataCnt << 8;
                    dataCnt             |= cmdBuffer[i+2] >> 8;     // lowest byte

                    statusByte      = cmdBuffer[i+2] & 0xff;

                    for(j=0; j<3; j++) {                            // clear this command 
                        cmdBuffer[i + j] = 0;               
                    }

                    state = STATE_DATA_READ;                        // go into DATA_READ state
                    i += 2;
                break;
        }
    }
}

void handleAcsiConfig(BYTE acsiIds, BYTE sdId)
{
    int j;
    
    firstConfigReceived = TRUE;                     // mark that we've received 1st config
    mode = MODE_WITH_RPI;                           // now that we got config, we're in 'with RPi' mode

    showCurrentLED();

    for(j=0; j<8; j++) {                            // for each bit in ids set the flag in enabledIDs[]
        if(acsiIds & (1<<j)) {
            enabledIDs[j] = TRUE;
        } else {
            enabledIDs[j] = FALSE;
        }
    }

    sdCardID = sdId;                                        // lower BYTE: SD card ACSI ID
    EE_WriteVariable(EEPROM_OFFSET_SDCARD_ID, sdCardID);    // store the new SD card ID to fake EEPROM
}

void handleFddConfig(BYTE enabledFddImages)
{
    int j;
    
    for(j=0; j<3; j++) {                                // for each bit in ids set the flag in enabledImgs[]
        if(enabledFddImages & (1<<j)) {
            enabledImgs[j] = TRUE;
        } else {
            enabledImgs[j] = FALSE;
        }
    }
    
    fixLedsByEnabledImgs();                             // if we are switched to not enabled floppy image, fix this
    showCurrentLED();                                   // and show the current LED
}

void handleFddSwitch(BYTE toWhichImage)
{
    if(toWhichImage <3) {                               // if the LED index is OK (0, 1, 2)
        if(enabledImgs[toWhichImage] == TRUE) {         // and this LED is enabled
            currentLed = toWhichImage;                  // store the new LED
        }
    }

    if(toWhichImage == 0xff) {                           // if this is a command to turn off the LED, store it
        currentLed = toWhichImage;
    }

    showCurrentLED();                                   // and light it up
}

// the interrupt on DMA SPI TX finished should minimize the need for checking and reseting ATN pin
void DMA1_Channel3_IRQHandler(void)
{
    DMA_ClearITPendingBit(DMA1_IT_TC3);                             // possibly DMA1_IT_GL3 | DMA1_IT_TC3

    GPIOA->BRR = ATN;                                               // ATTENTION bit low  - nothing to read

    spiDmaTXidle = TRUE;                                            // SPI DMA TX now idle
    
    if(spiDmaRXidle == TRUE) {                                      // and if even the SPI DMA RX is idle, SPI is idle completely
        spiDmaIsIdle = TRUE;                                        // SPI DMA is idle
        timeOutCount = 0;                                           // reset count of how many times we must have timed out to keep working
    }
}

// interrupt on Transfer Complete of SPI DMA RX channel
void DMA1_Channel2_IRQHandler(void)
{
    DMA_ClearITPendingBit(DMA1_IT_TC2);                             // possibly DMA1_IT_GL2 | DMA1_IT_TC2

    spiDmaRXidle = TRUE;                                            // SPI DMA RX now idle
    
    if(spiDmaTXidle == TRUE) {                                      // and if even the SPI DMA TX is idle, SPI is idle completely
        spiDmaIsIdle = TRUE;                                        // SPI DMA is idle
        timeOutCount = 0;                                           // reset count of how many times we must have timed out to keep working
    }
}

void spi_init1(void)
{
    SPI_InitTypeDef spiStruct;

    SPI_Cmd(SPI1, DISABLE);
    
    SPI_StructInit(&spiStruct);
    spiStruct.SPI_DataSize = SPI_DataSize_16b;                      // use 16b data size to lower the MCU load
    
    SPI_Init(SPI1, &spiStruct);
    SPI1->CR2 |= (1 << 7) | (1 << 6) | SPI_I2S_DMAReq_Tx | SPI_I2S_DMAReq_Rx;       // enable TXEIE, RXNEIE, TXDMAEN, RXDMAEN
    
    SPI_Cmd(SPI1, ENABLE);
}

void spi_init2(void)
{
    SPI_InitTypeDef spiStruct;

    SPI_Cmd(SPI2, DISABLE);
    
    SPI_StructInit(&spiStruct);
    
    spiStruct.SPI_Mode              = SPI_Mode_Master;
    spiStruct.SPI_DataSize          = SPI_DataSize_8b;
    spiStruct.SPI_NSS               = SPI_NSS_Soft;
    spiStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;                          // 72 MHz / 4 = 18 MHz 
    spiStruct.SPI_FirstBit          = SPI_FirstBit_MSB;

    SPI_Init(SPI2, &spiStruct);
    SPI2->CR2 |= (1 << 7) | (1 << 6) | SPI_I2S_DMAReq_Tx | SPI_I2S_DMAReq_Rx;           // enable TXEIE, RXNEIE, TXDMAEN, RXDMAEN
    
    SPI_Cmd(SPI2, ENABLE);
}

void dma_spi_init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    // DMA1 channel3 configuration -- SPI1 TX
    DMA_DeInit(DMA1_Channel3);
    
    DMA_InitStructure.DMA_MemoryBaseAddr      = (uint32_t) 0;                           // from this buffer located in memory (now only fake)
    DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32_t) &(SPI1->DR);                 // to this peripheral address
    DMA_InitStructure.DMA_DIR                 = DMA_DIR_PeripheralDST;                  // dir: from mem to periph
    DMA_InitStructure.DMA_BufferSize          = 0;                                      // fake buffer size
    DMA_InitStructure.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;              // PINC = 0 -- don't icrement, always write to SPI1->DR register
    DMA_InitStructure.DMA_MemoryInc           = DMA_MemoryInc_Enable;                   // MINC = 1 -- increment in memory -- go though buffer
    DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_HalfWord;        // each data item: 16 bits 
    DMA_InitStructure.DMA_MemoryDataSize      = DMA_MemoryDataSize_HalfWord;            // each data item: 16 bits 
    DMA_InitStructure.DMA_Mode                = DMA_Mode_Normal;                        // normal mode
    DMA_InitStructure.DMA_Priority            = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                 = DMA_M2M_Disable;                        // M2M disabled, because we move to peripheral
    DMA_Init(DMA1_Channel3, &DMA_InitStructure);

    // Enable interrupts
    DMA_ITConfig(DMA1_Channel3, DMA_IT_TC, ENABLE);                                     // interrupt on Transfer Complete (TC)

    //----------------
    // DMA1 channel2 configuration -- SPI1 RX
    DMA_DeInit(DMA1_Channel2);
    
    DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32_t) &(SPI1->DR);                 // from this peripheral address
    DMA_InitStructure.DMA_MemoryBaseAddr      = (uint32_t) 0;                           // to this buffer located in memory (now only fake)
    DMA_InitStructure.DMA_DIR                 = DMA_DIR_PeripheralSRC;                  // dir: from periph to mem
    DMA_InitStructure.DMA_BufferSize          = 0;                                      // fake buffer size
    DMA_InitStructure.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;              // PINC = 0 -- don't icrement, always write to SPI1->DR register
    DMA_InitStructure.DMA_MemoryInc           = DMA_MemoryInc_Enable;                   // MINC = 1 -- increment in memory -- go though buffer
    DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_HalfWord;        // each data item: 16 bits 
    DMA_InitStructure.DMA_MemoryDataSize      = DMA_MemoryDataSize_HalfWord;            // each data item: 16 bits 
    DMA_InitStructure.DMA_Mode                = DMA_Mode_Normal;                        // normal mode
    DMA_InitStructure.DMA_Priority            = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M                 = DMA_M2M_Disable;                        // M2M disabled, because we move from peripheral
    DMA_Init(DMA1_Channel2, &DMA_InitStructure);

    // Enable DMA1 Channel2 Transfer Complete interrupt
    DMA_ITConfig(DMA1_Channel2, DMA_IT_TC, ENABLE);           // interrupt on Transfer Complete (TC)
    
    //----------------
    // now enable interrupt on DMA1_Channel3
    NVIC_InitStructure.NVIC_IRQChannel                    = DMA1_Channel3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority         = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // and also interrupt on DMA1_Channel2
    NVIC_InitStructure.NVIC_IRQChannel                    = DMA1_Channel2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = 0x02;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority         = 0x02;
    NVIC_InitStructure.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void dma_spi2_init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
//    NVIC_InitTypeDef NVIC_InitStructure;
    
    // DMA1 channel5 configuration -- SPI2 TX
    DMA_DeInit(DMA1_Channel5);
    
    DMA_InitStructure.DMA_MemoryBaseAddr      = (uint32_t) 0;                           // from this buffer located in memory (now only fake)
    DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32_t) &(SPI2->DR);                 // to this peripheral address
    DMA_InitStructure.DMA_DIR                 = DMA_DIR_PeripheralDST;                  // dir: from mem to periph
    DMA_InitStructure.DMA_BufferSize          = 0;                                      // fake buffer size
    DMA_InitStructure.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;              // PINC = 0 -- don't icrement, always write to SPI1->DR register
    DMA_InitStructure.DMA_MemoryInc           = DMA_MemoryInc_Enable;                   // MINC = 1 -- increment in memory -- go though buffer
    DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_Byte;            // each data item: 16 bits 
    DMA_InitStructure.DMA_MemoryDataSize      = DMA_MemoryDataSize_Byte;                // each data item: 16 bits 
    DMA_InitStructure.DMA_Mode                = DMA_Mode_Normal;                        // normal mode
    DMA_InitStructure.DMA_Priority            = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                 = DMA_M2M_Disable;                        // M2M disabled, because we move to peripheral
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    // Enable interrupts
    DMA_ITConfig(DMA1_Channel5, DMA_IT_TC, ENABLE);                                     // interrupt on Transfer Complete (TC)

    //----------------
    // DMA1 channel4 configuration -- SPI2 RX
    DMA_DeInit(DMA1_Channel4);
    
    DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32_t) &(SPI2->DR);                 // from this peripheral address
    DMA_InitStructure.DMA_MemoryBaseAddr      = (uint32_t) 0;                           // to this buffer located in memory (now only fake)
    DMA_InitStructure.DMA_DIR                 = DMA_DIR_PeripheralSRC;                  // dir: from periph to mem
    DMA_InitStructure.DMA_BufferSize          = 0;                                      // fake buffer size
    DMA_InitStructure.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;              // PINC = 0 -- don't icrement, always write to SPI1->DR register
    DMA_InitStructure.DMA_MemoryInc           = DMA_MemoryInc_Enable;                   // MINC = 1 -- increment in memory -- go though buffer
    DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_Byte;            // each data item: 16 bits 
    DMA_InitStructure.DMA_MemoryDataSize      = DMA_MemoryDataSize_Byte;                // each data item: 16 bits 
    DMA_InitStructure.DMA_Mode                = DMA_Mode_Normal;                        // normal mode
    DMA_InitStructure.DMA_Priority            = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M                 = DMA_M2M_Disable;                        // M2M disabled, because we move from peripheral
    DMA_Init(DMA1_Channel4, &DMA_InitStructure);

    // Enable DMA1 Channel4 Transfer Complete interrupt
    DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);           // interrupt on Transfer Complete (TC)
    
    //----------------
/*
    // now enable interrupt on DMA1_Channel5
    NVIC_InitStructure.NVIC_IRQChannel                    = DMA1_Channel5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = 0x04;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority         = 0x04;
    NVIC_InitStructure.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // and also interrupt on DMA1_Channel4
    NVIC_InitStructure.NVIC_IRQChannel                    = DMA1_Channel4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority  = 0x05;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority         = 0x05;
    NVIC_InitStructure.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
*/
}

void setupAtnBuffers(void)
{
    atnSendFwVersion[0] = ATN_SYNC_WORD;                    // starting mark
    atnSendFwVersion[1] = ATN_FW_VERSION;                   // attention code
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    atnSendFwVersion[4] = version[0];
    atnSendFwVersion[5] = version[1];
    // WORD 6 is reserved for current LED (floppy image) number
    // WORD 7 and 8
    atnSendFwVersion[9] = 0;                                // terminating zero
    
    atnSendACSIcommand[0] = ATN_SYNC_WORD;                  // starting mark
    atnSendACSIcommand[1] = ATN_ACSI_COMMAND;               // attention code
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    atnSendACSIcommand[11] = 0;                             // terminating zero
    
    atnMoreData[0] = ATN_SYNC_WORD;                         // starting mark
    atnMoreData[1] = ATN_READ_MORE_DATA;                    // mark that we want to read more data
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    // WORD 4 is sequence number of the request for this command
    atnMoreData[5] = 0;
    
    wrBuf1.buffer[0]    = ATN_SYNC_WORD;                    // starting mark
    wrBuf1.buffer[1]    = ATN_WRITE_MORE_DATA;
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    // WORD 4 is sequence number of the request for this command
    wrBuf1.next             = (void *) &wrBuf2;

    wrBuf2.buffer[0]    = ATN_SYNC_WORD;                    // starting mark
    wrBuf2.buffer[1]    = ATN_WRITE_MORE_DATA;
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    // WORD 4 is sequence number of the request for this command
    wrBuf2.next         = (void *) &wrBuf1;

    atnGetStatus[0] = ATN_SYNC_WORD;                        // starting mark
    atnGetStatus[1] = ATN_GET_STATUS;
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    atnGetStatus[4] = 0;
    
    // now set the next pointers in read buffers
    rdBuf1.next = (void *) &rdBuf2;
    rdBuf2.next = (void *) &rdBuf1;
}

void getXilinxStatus(void)
{
    BYTE val;
    
    ACSI_DATADIR_WRITE();
    GPIOA->BSRR	= aPIO | aDMA;          // aPIO and aDMA now HIGH -- we can read the XILINX info byte afer that

    val         = GPIOB->IDR;           // read in the value

    GPIOA->BRR	= aPIO | aDMA;          // aPIO and aDMA now LOW -- back to normal state

    xilinxBusIdle   = val >> 7;         // highest bit to position of bit 0 - when 0 then SCSI is busy, when 1 then SCSI is idle
    xilinxHwFw      = val & 0x7f;       // store part as Info Byte
    hwVersion       = xilinxHwFw >> 4;  // get only HW version -- should be 2 for v.2, or 1 for v.1
    
    if(hwVersion == 2) {                // if it's HW v. 2
        if((xilinxHwFw & 0x03) == 2) {  // FW type: SCSI? 
            isAcsiNotScsi   = 0;
        } else {                        // FW type: ACSI
            isAcsiNotScsi   = 1;
        }
    } else {                            // if it's HW v. 1 - it's ACSI
        isAcsiNotScsi   = 1;
    }
}

void resetXilinx(void)
{
    BYTE tmp;
    
    ACSI_DATADIR_WRITE();
    GPIOA->BSRR = aRNW;                 // aRNW must go H for soft reset
    
    GPIOA->BSRR	= aPIO | aDMA;          // aPIO and aDMA now HIGH -- we can read the XILINX info byte afer that
    
    tmp         = GPIOB->IDR;           // read in the value -- just to make the reset / identify pulse longer
    
    GPIOA->BRR  = aRNW;                 // aRNW back to L, to match the rest of the bus direction
    GPIOA->BRR	= aPIO | aDMA;          // aPIO and aDMA now LOW -- back to normal state

    EXTI->PR    = aCMD | aCS | aACK;    // also clear possible EXTI flags
    
    brStat      = E_OK;                 // set bridge status to OK
}

BYTE isBusIdle(void)                    // get if the SCSI bus is idle (BSY high, REQ high) or busy
{
    getXilinxStatus();
    return xilinxBusIdle;
}

#ifdef CMDLOGGING

// start some command logging (if enabled)
void cmdLog_onStart(void)
{
	cmdLog[cmdLogIndex].cmdSeqNo				= cmdSeqNo;							// store sequence number of command
	memcpy((char *) cmdLog[cmdLogIndex].cmd, (char *) cmd, 14);	// copy the whole command
	cmdLog[cmdLogIndex].cmdLen					= cmdLen;								// also store the length
	cmdLog[cmdLogIndex].bridgeStatus		= brStat;								// store the bridge status byte
	cmdLog[cmdLogIndex].scsiStatusByte	= 0xff;									// and also some fake SCSI status byte (other than zero, which is for no-error)
}

void cmdLog_storeResults(BYTE bridgeStatus, BYTE scsiStatus)
{
	// update these retults
	cmdLog[cmdLogIndex].bridgeStatus		= bridgeStatus;
	cmdLog[cmdLogIndex].scsiStatusByte	= scsiStatus;
}

// finish some command logging on end of command (if enabled)
void cmdLog_onEnd(void)
{
	// update sequence number
	cmdSeqNo++;
	
	// update log field index
	cmdLogIndex++;
	if(cmdLogIndex >= CMDLOG_COUNT) {
		cmdLogIndex = 0;
	}
}
#endif
