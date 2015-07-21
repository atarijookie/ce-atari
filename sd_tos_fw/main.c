#include <stdio.h>

#include "init.h"
#include "global.h"
#include "sd_raw.h"
#include "sd_raw_config.h" 

BYTE isReset(void);
BYTE isButtonPressed(void);

#define TIME_100MS  10
#define TIME_200MS  20
#define TIME_300MS  20
#define TIME_500MS  50
#define TIME_1S     100
#define TIME_2S     200

#define bitClr(REG,BIT)     REG = REG & (~BIT)
#define bitSet(REG,BIT)     REG = REG | BIT

void mainLoopForMaster(void);
void mainLoopForSlave(void);

//-----------------------------
// for MASTER only
void handleButtonPress(void);
void generateBlinkPattern(void);

void blinkLed(void);
void setLedAccordingToCurrentPatternIndex(void);
void ledOn(void);
void ledOff(void);

void findTosImages(void);
void storeConfigToSDcard(void);

//-----------------------------

void sleep100ms(void);
void loadConfigFromSDcard(void);
void writeToSram(DWORD addr, BYTE data);

void loadTosImage(BYTE hiNotLow);
void setSramAddressHighest(BYTE A16_A9);

void memset(BYTE *b, BYTE val, WORD cnt);
BYTE memcmp(BYTE *a, BYTE *b,  WORD cnt);

void putHexByte(BYTE inp);
void putHexDWORD(DWORD inp);

void SD_CS_input(void);
void SD_CS_output(void);

//-----------------------------

struct {
    BYTE imgCurrent;        // index # of currently loaded image
    BYTE imgNext;           // index # of image we should load on next reset 
    BYTE imgMax;            // how many images there are on SD card

    BYTE masterNotSlave;    // non-zero if master, zero if slave
} config;

#define MAX_PATTERNS    100

struct {
    BYTE pattern[MAX_PATTERNS];
    BYTE patternMax;

    BYTE curIndex;
    BYTE curCount;
} ledBlink;

BYTE bfr[512];

struct TSDinfo SDinfo;

volatile BYTE tickCount;

void main(void)
{
    init_devices();

    puts("\ninit ");
    //-------------
    config.imgCurrent   = 0xff;         // no image loaded
    config.imgNext      = 0;            // should load image #0
    config.imgMax       = 0;            // there are 0 images on SD card (we have to find out!)

    if(PIND & IS_SLAVE) {               // IS_SLAVE pin is high? it's a master 
        puts("master ");
        config.masterNotSlave = TRUE;
    } else {                            // IS_SLAVE pin is low? it's a slave
        puts("slave ");
        config.masterNotSlave = FALSE;
    }
    
    //-------------
    // Roles of master and slave are bit different, that's why their code is separated.    

    if(config.masterNotSlave) {
        // Master: init SD card, crawl through the SD card for SD card images, handle the button, blink the LED, on ST RESET store the config to SD card and load the higher part of TOS into RAM
        mainLoopForMaster();
    } else {
        // Slave : wait for ST reset, then wait for SD card to be idle for some time, then load the lower part of TOS into RAM   
        mainLoopForSlave();
    }
}

////////////////////////////////////////////////////////////////////////////////

void mainLoopForMaster(void)
{
    // generate blink pattern for LED
    generateBlinkPattern();

    //-------------------------------------------
    // init SD card and get capacity
    puts("SD ");

    if(!sd_raw_init()) {                        // try to init the card
        // failed to init? Nothing to do!
        puts("fail\n");
        while(1);
    }

    puts("size ");
    
    if(!sd_raw_get_info(&SDinfo)) {             // try to get info
        // failed to get size?            
        puts("fail\n");
        while(1); 
    }
    
    putHexDWORD(SDinfo.capacity);
    puts("\n");
    //-------------------------------------------

    findTosImages();                            // find how many TOS images there are on SD card
    loadConfigFromSDcard();                     // load config from SD card

    while(1) {
        if(isReset()) {                         // if ST is in reset state
            if(config.imgCurrent != config.imgNext) {   // TOS image change requested? Do load... Otherwise quit
                SD_CS_output();                 // SD CS as output, because we will work with SD card

                storeConfigToSDcard();          // store the config, so we can start the right one on next power off/on cycle. Also slave needs to know this.
                loadTosImage(1);                // load the TOS image - HI part

                SD_CS_input();                  // SD CS as input (let it float), because SLAVE will work with this wire
            }
        } else {                                // if not reset
            SD_CS_output();                     // SD CS as output, because it controlls the LED

            if(isButtonPressed()) {             // if button pressed
                handleButtonPress();            // switch to higher TOS image
                generateBlinkPattern();         // generate the new LED blink pattern
            }

            blinkLed();                         // blink LED
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void mainLoopForSlave(void)
{
    SD_CS_input();                                      // SD CS as input (let it float), because MASTER will work with SD card and LED, until it's reset and TOS is loaded 

    while(1) {
        if(isReset()) {                                 // if ST is in RESET state
            //--------------------------------
            // first wait until the SD CS line is IDLE for 0.5 s 
            SD_CS_input();                              // SD CS as input, because we need to see if the SD card is not idle

            tickCount = 0;
            while(1) {
                BYTE sdCs = (PINB & SD_CS);             // get only CS line

                if(sdCs == 0) {                         // SD CS is low? restart waiting for IDLE line
                    tickCount = 0;
                }

                if(tickCount >= TIME_500MS) {           // if the SD CS line is idle for at least 500 ms, we can quit this waiting and continue with loading the TOS image
                    break;
                }

                if(!isReset()) {                        // if the ST came out of RESET mode, quit - we can't load the image now...
                    break;
                }
            }

            //--------------------------------
            // if it's still ST RESET, and need to change TOS image...
            if(isReset() && (config.imgCurrent != config.imgNext)) {
                SD_CS_output();                         // SD CS as output, because we will work with SD card now

                loadConfigFromSDcard();                 // load config from SD card
                loadTosImage(0);                        // load the TOS image - LO part

                SD_CS_input();                          // SD CS as input, because after reset MASTER will blink with the LED
            }
        } else {                                        // not ST RESET? 
            SD_CS_input();                              // SD CS as input, because MASTER controlls that line when it's not ST RESET 
        } 
    }
}

////////////////////////////////////////////////////////////////////////////////

void findTosImages(void)
{
    WORD  imgNo;
    DWORD startingSector = 0;
    BYTE  foundImages = 0;
    BYTE  res;

    puts("images: ");

    for(imgNo=0; imgNo<50; imgNo++) {
        memset(bfr, 0, 16);                     // clear buffer
        res = sd_raw_read(startingSector, bfr); // try to read sector

        if(!res) {                              // failed to read sector? quit
            break;
        }

        res = memcmp(bfr, "TOS_IMAGE!", 10);    // compare the buffer to expected tag

        if(res) {                               // non-zero value means tag not found
            break;
        }

        foundImages++;                          // consider this as a valid image
        startingSector += 2048;                 // jump 1 MB further on the card (2048 sectors further)
    }

    config.imgMax = foundImages;                // store how many images we've found

    putHexByte(config.imgMax);
    puts("\n");
}
 
// load config from SD card - config.imgNext (# of image which should be loaded next)   
void loadConfigFromSDcard(void)
{
    BYTE  res;

    puts("config: ");

    config.imgNext = 0;                         // read image #0

    memset(bfr, 0, 32);                         // clear buffer
    res = sd_raw_read(0, bfr);                  // try to read sector - sector #0 contains also config

    if(!res) {                                  // failed to read sector? quit
        puts("0 - fail\n");
        return;
    }

    res = memcmp(bfr, "TOS_IMAGE!", 10);        // compare the buffer to expected tag

    if(res) {                                   // non-zero value means tag not found
        puts("0 - not found\n");
        return;
    }

    config.imgNext = bfr[10];                   // byte #10: number of TOS image we should load
    
    if(config.imgNext >= config.imgMax) {       // if image # out of range, fix it
        config.imgNext = 0;
    }

    putHexByte(config.imgNext);
    puts("\n");
}

// store config to SD card - config.imgNext (# of image which should be loaded next)   
void storeConfigToSDcard(void)
{
    BYTE  res;

    puts("store ");
    putHexByte(config.imgNext);
    puts(": ");

    config.imgNext = 0;                         // read image #0

    memset(bfr, 0, 32);                         // clear buffer
    res = sd_raw_read(0, bfr);                  // try to read sector - sector #0 contains also config

    if(!res) {                                  // failed to read sector? quit
        puts("read fail\n");
        return;
    }

    res = memcmp(bfr, "TOS_IMAGE!", 10);        // compare the buffer to expected tag

    if(res) {                                   // non-zero value means tag not found, thus don't save!
        puts("tag fail\n");
        return;
    }

    bfr[10] = config.imgNext;                   // byte #10: number of TOS image we should load
    res = sd_raw_write(0, bfr);                 // try to write sector - sector #0 contains also config

    if(!res) {
        puts("write fail\n");
    } else {
        puts("write good\n");
    }
}

void loadTosImage(BYTE hiNotLow)
{
    DWORD s;
    BYTE res;
    WORD i, j;
    DWORD addr;

    portInit_outputBus();                       // switch pin directions so we can write to SRAM
    puts("TOS ");

    //--------------
    // first verify that the TOS image is there
    s = config.imgNext * 2048;                  // calculate where the TOS image starts on card - each image has 2048 sectors (1MB) reserved

    memset(bfr, 0, 32);                         // clear buffer
    res = sd_raw_read(s, bfr);                  // try to read 0th sector  

    if(!res) {                                  // failed to read sector? quit
        puts("1st read fail\n");
        portInit_floatBus();                    // switch pin directions so we won't interfere with ST bus
        return;
    }

    res = memcmp(bfr, "TOS_IMAGE!", 10);        // compare the buffer to expected tag

    if(res) {                                   // non-zero value means tag not found, thus don't save!
        puts("tag fail\n");
        portInit_floatBus();                    // switch pin directions so we won't interfere with ST bus
        return;
    }

    //--------------
    // now try to load it
    s++;                                        // skip the TAG sector

    if(hiNotLow) {                              // if it's HI (not LO) ROM, then it starts 256 sectors further
        s += 256;
    }

    addr = 0;                                   // start loading to SRAM address 0

    for(i=0; i<256; i++) {                      // load 256 sectors (128 kB)
        res = sd_raw_read(s++, bfr);

        if(!res) {
            puts("load fail\n");
            portInit_floatBus();                // switch pin directions so we won't interfere with ST bus
            return;
        }

        for(j=0; j<512; j++) {                  // write the whole buffer to SRAM
            writeToSram(addr++, bfr[j]); 
        }
    } 

    config.imgCurrent = config.imgNext;         // store that we succeeded to load the image
    portInit_floatBus();                        // switch pin directions so we won't interfere with ST bus

    puts("good\n");
}

#define OUT_A(X,Y)     if(X) { bitSet(PORTA, Y); } else { bitClr(PORTA, Y); }
#define OUT_B(X,Y)     if(X) { bitSet(PORTB, Y); } else { bitClr(PORTB, Y); }
#define OUT_C(X,Y)     if(X) { bitSet(PORTC, Y); } else { bitClr(PORTC, Y); }
#define OUT_D(X,Y)     if(X) { bitSet(PORTD, Y); } else { bitClr(PORTD, Y); }  

void writeToSram(DWORD addr, BYTE data)
{
    static BYTE highestAddr = 0xff;
    BYTE A16_A9 = addr >> 9;                    // get 8 highest bits

    bitSet(PORTC, OE);                          // OE high - don't output data
    bitClr(PORTC, CS);                          // CS low  - select this chip

    //----------------------
    // if the last set highest bits don't match what we need now, set it
    if(highestAddr != A16_A9) {                 
        setSramAddressHighest(A16_A9);          // set the highest address bits through SIPO register
        highestAddr = A16_A9;
    }

    //----------------------
    // set the DATA and ADDRESS pins as they need to be
    OUT_A(addr & (1 << 6), A6); 
    OUT_A(addr & (1 << 5), A5); 
    OUT_A(addr & (1 << 4), A4); 
    OUT_A(addr & (1 << 3), A3); 
    OUT_A(addr & (1 << 1), A1); 
    OUT_A(addr & (1 << 2), A2); 
    OUT_A(addr & (1 << 0), A0); 
    OUT_A(data & (1 << 0), D0);

    OUT_B(addr & (1 << 1), A1); 
    OUT_B(data & (1 << 3), D3); 
    OUT_B(data & (1 << 2), D2); 
    OUT_B(data & (1 << 1), D1);

    OUT_C(addr & (1 << 7), A7); 
    OUT_C(addr & (1 << 8), A8); 
    OUT_C(data & (1 << 7), D7); 
    OUT_C(data & (1 << 6), D6);

    OUT_D(data & (1 << 5), D5); 
    OUT_D(data & (1 << 4), D4);

    //----------------------
    // now WE low and hi, to write the data into the SRAM
    bitClr(PORTC, WE);
    bitSet(PORTC, WE);
}

#define SIPO_OUT(X)     { if(X) { bitSet(PORTC, SIPO_D); } else { bitClr(PORTC, SIPO_D); } };  bitSet(PORTD, SIPO_CLK);  bitClr(PORTD, SIPO_CLK);

void setSramAddressHighest(BYTE A16_A9)
{
    bitClr(PORTD, SIPO_CLK);

    SIPO_OUT(A16_A9 & (1 << 7));        // A16
    SIPO_OUT(A16_A9 & (1 << 6));        // A15
    SIPO_OUT(A16_A9 & (1 << 3));        // A12
    SIPO_OUT(A16_A9 & (1 << 1));        // A10
    SIPO_OUT(A16_A9 & (1 << 2));        // A11
    SIPO_OUT(A16_A9 & (1 << 0));        // A9
    SIPO_OUT(A16_A9 & (1 << 4));        // A13
    SIPO_OUT(A16_A9 & (1 << 5));        // A14

    // add extra CLK pulse so the data would pass from SIPO to output REGISTER
    bitSet(PORTD, SIPO_CLK);  
    bitClr(PORTD, SIPO_CLK);
}

void handleButtonPress(void)
{
    //---------------
    // wait until button is released
    while(1) {
        while(isButtonPressed());   // wait here while the button is still pressed

        sleep100ms();               // now wait 100 ms just to be sure any / most bounces will go away
        
        if(!isButtonPressed()) {    // if the button is not pressed, we can quit this loop
            break;
        }
    }

    //---------------
    // switch to the higher TOS image
    config.imgNext++;
    if(config.imgNext >= config.imgMax) {   // fix if would go out of range
        config.imgNext = 0;
    }
}

void generateBlinkPattern(void)
{
    BYTE ind = 0, i;
    BYTE blinkCount; 

    //----------------------------------
    // nothing loaded?
    if(config.imgCurrent = 0xff) {              
        ledBlink.pattern[0] = TIME_500MS;       // 500 ms on
        ledBlink.pattern[1] = TIME_500MS;       // 500 ms off
        ledBlink.patternMax = 2;

        ledBlink.curIndex   = 0;
        ledBlink.curCount   = ledBlink.pattern[0]; 
        return;
    }
    
    //----------------------------------
    // figure out how many times we will blink with LED (one TOS image will produce 1 blink, which needs 2 slots in ledBlink.pattern[] - 1x ON, 1x OFF)
    blinkCount = config.imgCurrent + 1;

    if((blinkCount * 2) > (MAX_PATTERNS - 2)) { // Limit the blink count to max patters we can store. What kind of sick person would store 50 TOS images to the card??? :)
        blinkCount = (MAX_PATTERNS - 2) / 2;
    }

    //----------------------------------
    // image will be changed? Invert blinking by adding a short ON period at the start
    if(config.imgCurrent != config.imgNext) {   
        ledBlink.pattern[ind++] = 1;            // short ON period
    }

    //----------------------------------
    // now add all the blinks represending selected TOS image
    for(i=0; i<blinkCount; i++) {
        ledBlink.pattern[ind++] = TIME_200MS;   // ON  for 200 ms
        ledBlink.pattern[ind++] = TIME_300MS;   // OFF for 300 ms
    }

    ledBlink.pattern[ind - 1] = TIME_2S;        // change last period to 2 seconds - to distinguish between the loops of blinks

    ledBlink.patternMax = ind;                  // store how many intervals we have
    ledBlink.curIndex   = 0;                    // start from the beginning
    ledBlink.curCount   = ledBlink.pattern[0];  // and with the 0th interval

    tickCount = 0;                              // start time from 0
}

void blinkLed(void)
{
    setLedAccordingToCurrentPatternIndex();

    if(tickCount < ledBlink.curCount) {             // if not enough time has passed, do nothing
        return;
    }

    ledBlink.curIndex++;                            // go to next blink interval
    if(ledBlink.curIndex >= ledBlink.patternMax) {  // went out of range? Fix it.
        ledBlink.curIndex = 0;
    }

    ledBlink.curCount = ledBlink.pattern[ledBlink.curIndex];    // store the interval to which we should go
    tickCount = 0;                                  // start time from 0

    setLedAccordingToCurrentPatternIndex();
}

void setLedAccordingToCurrentPatternIndex(void)
{
    if(ledBlink.curIndex & 1) {                 // ODD  index? LED off
        ledOff();
    } else {                                    // EVEN index? LED on 
        ledOn();
    }
}

//------------------------------------------------------------------------------

void ledOn(void)
{
    bitClr(PORTB, SD_CS); 
}

void ledOff(void)
{
    bitSet(PORTB, SD_CS); 
}

BYTE isButtonPressed(void)
{
    BYTE val = PIND;
    if(val & BTN) {
        return FALSE;
    }

    return TRUE;
}

BYTE isReset(void)
{
    BYTE val = PIND;
    
    if(val & ST_RESET) {        // bit high? not reset
        return FALSE;
    }

    return TRUE;
}

// 100 Hz timer
#pragma interrupt_handler timer0_ovf_isr:10
void timer0_ovf_isr(void)
{
    TCNT0 = 0xD9;   //reload counter value

    tickCount++;    // increment tick count
}

void sleep100ms(void)
{
    tickCount = 0;              // reset tick count to 0

    // Now wait until tickCount goes to 10. At 100 Hz timer this 10 means 100 ms... 
    while(tickCount <= TIME_100MS);
}

void memset(BYTE *b, BYTE val, WORD cnt)
{
    WORD i;

    for(i=0; i<cnt; i++) {
        b[i] = val;
    }
}

BYTE memcmp(BYTE *a, BYTE *b, WORD cnt)
{
    WORD i;

    for(i=0; i<cnt; i++) {
        if(a[i] != b[i]) {      // if found a difference, return non-zero 
            return 1;
        }
    }

    return 0;                   // no difference, return zero
}

//---------------------------------
void putHexByte(BYTE inp)
{
    BYTE Lo, Hi;
 
    Hi = (inp & 0xf0) >> 4;
    Lo = (inp & 0x0f);
 
    if(Hi<10) {         //  0..9  - convert to ASCII numner
     	Hi += 48;
    } else {            // 10..15 - convert to ASCII character
     	Hi += 55;
    }
 
    if(Lo<10) {         //  0..9  - convert to ASCII numner
     	Lo += 48;
    } else {            // 10..15 - convert to ASCII character
     	Lo += 55;
    }

    putchar(Hi);
    putchar(Lo);
}
//---------------------------------
void putHexDWORD(DWORD inp)
{
    BYTE a,b,c,d;

    a = inp >> 24;
    b = inp >> 16;
    c = inp >>  8;
    d = (BYTE) inp;

    putHexByte(a);
    putHexByte(b);
    putHexByte(c);
    putHexByte(d);
}
//---------------------------------
void SD_CS_input(void)
{
    bitClr(DDRB,    SD_CS);     // pin direction: input 
    bitClr(PORTB,   SD_CS);     // pull up: off
}
//---------------------------------
void SD_CS_output(void)
{
    bitSet(DDRB,    SD_CS);     // pin direction: output 
    bitSet(PORTB,   SD_CS);     // pin level: high
}
//---------------------------------

