#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifndef ONPC
    #include <bcm2835.h>
#endif

#include "spisd.h"
#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"

extern THwConfig hwConfig;

SpiSD::SpiSD()
{
#ifndef ONPC
    bcm2835_spi_begin();

    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);        // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                     // The default

    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024);    // 1024 => 244 kHz or 390 kHz

    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);        // the default
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);

    bcm2835_gpio_fsel(PIN_TCK,  BCM2835_GPIO_FSEL_OUTP);        // TCK
#endif

    txBufferFF = new BYTE[1024];
    memset(txBufferFF, 0xff, 1024);

    rxBuffer = new BYTE[1024];
}

SpiSD::~SpiSD()
{
    delete []txBufferFF;
    delete []rxBuffer;
}

void SpiSD::spiSetFrequency(BYTE highNotLow)
{
#ifndef ONPC
    if(highNotLow == SPI_FREQ_HIGH) {               // high frequency? 18 MHz (72 / 4)
        bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_16);      // 16 =  64ns = 15.625MHz
    } else {                                        // low frequency? 0.56 MHz (72 / 128)
        bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024);    // 1024 => 244 kHz or 390 kHz
    }
#endif
}

void SpiSD::clearStruct(void)
{
    isInit       = false;                                            
    type         = DEVICETYPE_NOTHING;
    mediaChanged = false;
    BCapacity    = 0;
	SCapacity    = 0;
}

void SpiSD::init(void)
{
    WORD i;
    BYTE res, stat;
    DWORD dRes;
    
    timeoutStart();

    //--------------------------
    // and now try to init all that is not initialized
    spiSetFrequency(SPI_FREQ_LOW);          // low SPI frequency

    res = reset();                          // init the MMC card
            
    if(res == DEVICETYPE_NOTHING) {         // if failed to init
        return;
    }

    // now to test if we can read from the card
    stat = mmcReadJustForTest(0);        

    if(stat) {
        return;
    }                       

    // get card capacity in blocks 
    if(res == DEVICETYPE_SDHC) {
        dRes = SDHC_Capacity();
    } else {
        dRes = MMC_Capacity();
    }
                
    type      = res;              // store device type
    SCapacity = dRes;             // store capacity in sectors
    BCapacity = dRes << 9;        // store capacity in bytes
    isInit    = true;             // set the flag
    mediaChanged = true;
    
    spiSetFrequency(SPI_FREQ_HIGH); 
}
//-----------------------------------------------
// It seems that on some cards the init sequence (or something from it) might cause the card
// to want to talk a little bit more than expected, and thus the 1st command issued to this 
// specific card might fail. For this situation just sending FFs and reading back the results
// might cure the situation. (Happened to me on 1 out of 5 cards, other 4 were working fine). 

void SpiSD::tryToEmptySdCardBuffer(void)
{
    WORD j;

    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 2);

    spiCSlow();
    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 1024);
 
    spiCShigh();
    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 2);
}

BYTE SpiSD::reset(void)
{
    BYTE r1;
    BYTE buff[5];
    int  devType = DEVICETYPE_NOTHING;
    
    spiSetFrequency(SPI_FREQ_LOW);
    spiCShigh();
    
    // send dummy bytes with CS high before accessing
    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 10);

    //---------------
    // now send the card to IDLE state  
    r1 = mmcSendCommand(MMC_GO_IDLE_STATE, 0);
    
    if(r1 != 1) {
        return DEVICETYPE_NOTHING;
    }

    //---------------
 
    r1 = mmcSendCommand5B(SDHC_SEND_IF_COND, 0x1AA, buff);         // init SDHC card
    
    if(r1 == 0x01)              // if got the right response -> it's an SD or SDHC card
    {
        if(buff[3] == 0x01 && buff[4] == 0xaa)      // if the rest of R7 is OK (OK echo and voltage)
        {
            while(true)
            {
                if(timeout()) {
                    return DEVICETYPE_NOTHING;
                }

                r1 = mmcSendCommand(SD_APP_CMD, 0); // ACMD41 = CMD55 + CMD41

                if(r1 != 1) {                       // if invalid reply to CMD55, then it's MMC card
                    devType = DEVICETYPE_MMC;
                    break;          
                }

                r1 = mmcSendCommand(SD_SEND_OP_COND, 0x40000000); // ACMD41 = CMD55 + CMD41 ---  HCS (High Capacity Support) bit set

                if(r1 == 0) {                       // if everything is OK, then it's SD card
                    devType = DEVICETYPE_SD;
                    break;
                }
            }
            //--------------
            r1 = mmcSendCommand5B(MMC_READ_OCR, 0, buff);          

            if(r1 == 0)                         // if command succeeded
            {
                if(buff[1] & 0x40) {            // if CCS bit is set -> SDHC card, else SD card
                    return DEVICETYPE_SDHC;
                }
            }
        }       
    }
    
    if(devType == DEVICETYPE_SD && r1 != 0) {     // if it's SD but failed to initialize
        return DEVICETYPE_NOTHING;
    }
        
    //-------------------------------
    if(devType == DEVICETYPE_MMC) {
        // try to initialize the MMC card
        r1 = mmcCmdLow(MMC_SEND_OP_COND, 0, 0);
    
        if(r1 != 0) {
            return DEVICETYPE_NOTHING;
        }
    }
    
    // set block length to 512 bytes
    r1 = mmcSendCommand(MMC_SET_BLOCKLEN, 512);

    return devType;
}
//-----------------------------------------------
BYTE SpiSD::mmcCmd(BYTE cmd, DWORD arg, BYTE val)
{
    BYTE r1;

    do {
        r1 = mmcSendCommand(cmd, arg);
              
        if(timeout()) {
            return 0xff;
        }

    } while(r1 != val);
    
    return r1;
}
//-----------------------------------------------
BYTE SpiSD::mmcCmdLow(BYTE cmd, DWORD arg, BYTE val)
{
    BYTE r1;
    int i;

    spiCSlow();          // CS to L
    
    do
    {
        // issue the command
        r1 = mmcSendCommand(cmd, arg);

        bcm2835_spi_transfer(0xff);

        if(timeout()) {
            bcm2835_spi_transfernb(txBufferFF, rxBuffer, 10);

            spiCShigh();         // CS to H
            return 0xff;
        }

    } while(r1 != val);

    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 10);
    
    spiCShigh();         // CS to H
    
    return r1;
}
//-----------------------------------------------
BYTE SpiSD::mmcSendCommand(BYTE cmd, DWORD arg)
{
    BYTE r1;

    // assert chip select
    spiCSlow();          // CS to L

    bcm2835_spi_transfer(0xff);

    // issue the command
    r1 = mmcCommand(cmd, arg);

    spiCShigh_ff_return(r1);    // send 0xff, CS high, return value
}
//-----------------------------------------------
BYTE SpiSD::mmcSendCommand5B(BYTE cmd, DWORD arg, BYTE *buff)
{
    BYTE r1, i;

    // assert chip select
    spiCSlow();          // CS to L

    bcm2835_spi_transfer(0xff);

    // issue the command
    r1 = mmcCommand(cmd, arg);
    buff[0] = r1;
    
    // receive the rest of R7 register  
    bcm2835_spi_transfernb(txBufferFF, buff+1, 4);

    spiCShigh_ff_return(r1);    // send 0xff, CS high, return value
}
//-----------------------------------------------
BYTE SpiSD::mmcCommand(BYTE cmd, DWORD arg)
{
    BYTE r1;

    // construct command
    BYTE bfr[7];
    bfr[0] = 0xff;
    bfr[1] = cmd | 0x40, 
    bfr[2] = arg >> 24; 
    bfr[3] = arg >> 16; 
    bfr[4] = arg >>  8; 
    bfr[5] = arg; 
    bfr[6] = (cmd == SDHC_SEND_IF_COND) ? 0x86 : 0x95;  // crc valid only for: SDHC_SEND_IF_COND / MMC_GO_IDLE_STATE

    // send command
    bcm2835_spi_transfernb(bfr, rxBuffer, 7);
    
    // end command
    // wait for response
    // if more than 8 retries, card has timed-out
    // return the received 0xFF
    while(true)
    {
        r1 = bcm2835_spi_transfer(0xff);

        if(r1 != 0xff)
            break;
        
        if(timeout()) {
            break;
        }
    }
    
    // return response
    return r1;
}
//**********************************************************************
BYTE SpiSD::mmcRead(DWORD sector)
{
    BYTE r1;
    DWORD i;
    BYTE byte;
    BYTE failed = 0;

    timeoutStart();
    
    // assert chip select
    spiCSlow();          // CS to L

    if(type != DEVICETYPE_SDHC)                  // for non SDHC cards change sector into address
        sector = sector<<9;

    // issue command
    r1 = mmcCommand(MMC_READ_SINGLE_BLOCK, sector);

    // check for valid response
    if(r1 != 0x00) {
        spiCShigh_ff_return(r1);    // send 0xff, CS high, return value
    }

    //------------------------
    // wait for block start
    bool fail = false;

    while(i)
    {
        r1 = spi2_TxRx(0xFF);                            // get byte

        if(r1 == MMC_STARTBLOCK_READ)                   // if it's the wanted byte
            break;
        
        if(timeout()) {
            fail = true;
            break;
        }
    }
    //------------------------
    if(fail) {                                       // timeout?
        spiCShigh_ff_return(0xff);    // send 0xff, CS high, return value
    }
    //------------------------
    // read in data
    ACSI_DATADIR_READ();

    // get whole sector into rxBuffer
    // TODO: pass sector data to caller
    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 512);

    // read 16-bit CRC and add 1 padding byte
    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 3);

    spiCShigh();         // CS to H

///////////////////////////////////
// !!! THIS HELPS TO SYNCHRONIZE THE THING !!!
    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 3);
///////////////////////////////////

    // return success
    return 0;   
}
//-----------------------------------------------
BYTE SpiSD::mmcReadMore(DWORD sector, WORD count)
{
    BYTE r1, quit;
    WORD i,j;
    BYTE byte;

    timeoutStart();
    
    // assert chip select
    spiCSlow();                                             // CS to L

    if(type != DEVICETYPE_SDHC)                      // for non SDHC cards change sector into address
        sector = sector<<9;

    // issue command
    r1 = mmcCommand(MMC_READ_MULTIPLE_BLOCKS, sector);

    // check for valid response
    if(r1 != 0x00) {
        spiCShigh_ff_return(r1);    // send 0xff, CS high, return value
    }

    // read in data
    quit = 0;
    
    for(j=0; j<count; j++)                                  // read this many sectors
    {
        // wait for block start
        while(true) {
            r1 = bcm2835_spi_transfer(0xff);

            if(r1 == MMC_STARTBLOCK_READ) {
                break;
            }
        }

        // read sector from SD card
        // TODO: pass rxBuffer to caller
        bcm2835_spi_transfernb(txBufferFF, rxBuffer, 512);
       
        //---------------
        if(j == (count - 1))                                // if we've read the last sector
            break;
        
        // if we need to read more, then just read 16-bit CRC
        bcm2835_spi_transfernb(txBufferFF, rxBuffer, 2);
    }
    //-------------------------------
    // stop the transmition of next sector
    mmcCommand(MMC_STOP_TRANSMISSION, 0);               // send command instead of CRC

    spiCShigh_ff_return(0);    // send 0xff, CS high, return value
}
//-----------------------------------------------
BYTE SpiSD::mmcWriteMore(DWORD sector, WORD count)
{
    BYTE r1, quit;
    WORD i,j;
    DWORD thisSector;

    timeoutStart();
    
    // assert chip select
    spiCSlow();          // CS to L

    thisSector = sector;
    
    if(type != DEVICETYPE_SDHC)          // for non SDHC cards change sector into address
        sector = sector<<9;

    //--------------------------
    // issue command
    r1 = mmcCommand(MMC_WRITE_MULTIPLE_BLOCKS, sector);

    // check for valid response
    if(r1 != 0x00) {
        spiCShigh_ff_return(r1);    // send 0xff, CS high, return value
    }
    
    //--------------
    // read in data
    for(j=0; j<count; j++)                      // read this many sectors
    {
        if(thisSector >= sdCard.SCapacity)      // sector out of range?
        {
            quit = 1;
            break;                              // quit
        }
        //--------------            
        while(true) {                           // wait while busy
            r1 = bcm2835_spi_transfer(0xff);

            if(r1 == 0xff) {
                break;
        }

        bcm2835_spi_transfer(MMC_STARTBLOCK_MWRITE);        // 0xfc as start write multiple blocks

        // TODO: instead of txBufferFF send some input buffer to card
        bcm2835_spi_transfernb(txBufferFF, rxBuffer, 512);
        
        // send more: 16-bit CRC
        bcm2835_spi_transfernb(txBufferFF, rxBuffer, 2);

        thisSector++;                           // increment real sector #
    }
    //-------------------------------
    // TODO: wait while busy
    while(spi2_TxRx(0xFF) != 0xff);              // while busy

    spi2_TxRx(MMC_STOPTRAN_WRITE);               // 0xfd to stop write multiple blocks

    // TODO: wait while busy
    while(spi2_TxRx(0xFF) != 0xff);              // while busy

    //-------------------------------
    // for the MMC cards send the STOP TRANSMISSION also
    if(type == DEVICETYPE_MMC)       
    {
        mmcCommand(MMC_STOP_TRANSMISSION, 0);

        // TODO: wait while busy
        while(spi2_TxRx(0xFF) != 0xff);          // while busy
    }

    //-------------------------------
    spiCShigh_ff_return(0);    // send 0xff, CS high, return value
}
//-----------------------------------------------
BYTE SpiSD::mmcReadJustForTest(DWORD sector)
{
    BYTE r1;
    DWORD i;

    timeoutStart();
    
    // assert chip select
    spiCSlow();          // CS to L

    if(type != DEVICETYPE_SDHC)          // for non SDHC cards change sector into address
        sector = sector<<9;

    // issue command
    r1 = mmcCommand(MMC_READ_SINGLE_BLOCK, sector);

    // check for valid response
    if(r1 != 0x00) {
        spiCShigh_ff_return(r1);    // send 0xff, CS high, return value
    }

    //------------------------
    // wait for block start
    while(true)
    {
        r1 = bcm2835_spi_transfer(0xff);

        if(r1 == MMC_STARTBLOCK_READ)      // if it's the wanted byte
            break;
        
        if(timeout()) {
            spiCShigh_ff_return(0xff);    // send 0xff, CS high, return value
        }
    }
    //------------------------
    // read in data and don't care about them + 16 bit crc + padding 1 byte
    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 515);

    spiCShigh();                                // CS to H

    // return success
    return 0;   
}
//-----------------------------------------------
BYTE SpiSD::mmcWrite(DWORD sector)
{
    BYTE r1;
    DWORD i;
    
    timeoutStart();
    
    //-----------------
    // assert chip select
    spiCSlow();                                 // CS to L
    
    if(type != DEVICETYPE_SDHC)          // for non SDHC cards change sector into address
        sector = sector<<9;
        
    // issue command
    r1 = mmcCommand(MMC_WRITE_BLOCK, sector);

    // check for valid response
    if(r1 != 0x00) {
        spiCShigh_ff_return(r1);    // send 0xff, CS high, return value
    }
    
    // send dummy
    bcm2835_spi_transfer(0xff);

    // send data start token
    bcm2835_spi_transfer((MMC_STARTBLOCK_WRITE);
    // write data

    // TODO: send data not txBufferFF
    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 512);

    // write 16-bit CRC (dummy values)
    bcm2835_spi_transfernb(txBufferFF, rxBuffer, 2);
    
    // read data response token
    r1 = bcm2835_spi_transfer(0xff);

    if( (r1 & MMC_DR_MASK) != MMC_DR_ACCEPT) {
        spiCShigh_ff_return(r1);    // send 0xff, CS high, return value
    }

    // wait until card not busy
    //------------------------
    // wait for block start
    BYTE res = 0;

    while(true)
    {
        res = bcm2835_spi_transfer(0xff);

        if(res != 0) {                          // if it's the wanted byte
            res = 0;                            // success            
            break;
        }
        
        if(timeout()) {
            res = 0xff;
            break;
        }
    }

    //------------------------
    spiCShigh_ff_return(res);    // send 0xff, CS high, return value
}
//--------------------------------------------------------
/** 
*   Retrieves the CSD Register from the mmc 
* 
*   @return      Status response from cmd 
**/ 
BYTE SpiSD::MMC_CardType(unsigned char *buff) 
{ 
    BYTE byte, i; 

    // assert chip select
    spiCSlow();                     // CS to L

    // issue the command
    byte = mmcCommand(MMC_SEND_CSD, 0);

    if (byte!=0) {                     // if error 
        spiCShigh_ff_return(byte);      // send 0xff, CS high, return value
    } 

    // wait for block start
    for(i=0; i<16; i++) {
        byte = bcm2835_spi_transfer(0xff);
     
        if(byte == MMC_STARTBLOCK_READ)
            break;
    }

    if (byte!=MMC_STARTBLOCK_READ) {   // if error 
        spiCShigh_ff_return(byte);      // send 0xff, CS high, return value
    } 

    // read the data
    bcm2835_spi_transfernb(txBufferFF, buff, 16);

    spiCShigh_ff_return(0);            // send 0xff, CS high, return value
} 
//--------------------------------------------------------
DWORD SpiSD::SDHC_Capacity(void) 
{ 
 BYTE byte, blk_len; 
 DWORD c_size; 
 DWORD sectors; 
 BYTE buff[16];
 
    timeoutStart();
 
    byte = MMC_CardType(buff); 

    if(byte!=0)                     // if failed to get card type
        return 0xffffffff;

    blk_len = 0x0F & buff[5];       // this should ALWAYS BE equal 9 on SDHC

    if(blk_len != 9)
        return 0xffffffff;

    c_size = ((DWORD)(buff[7] & 0x3F) << 16) | ((DWORD)buff[8] << 8) | ((DWORD)(buff[9]));
    sectors = (c_size+1) << 10;     // get MaxSectors -> mcap=(csize+1)*512k -> msec=mcap/BytsPerSec(fix)

    return sectors; 
}
//--------------------------------------------------------
/** 
*   Calculates the capacity of the MMC in blocks 
* 
*   @return   uint32 capacity of MMC in blocks or -1 in error; 
**/ 
DWORD SpiSD::MMC_Capacity(void) 
{ 
 BYTE byte,data, multi, blk_len; 
 DWORD c_size; 
 DWORD sectors; 
 BYTE buff[16];
 
 timeoutStart();
 
 byte = MMC_CardType(buff); 

 if (byte!=0)
    return 0xffffffff;
    
 // got info okay 
 blk_len = 0x0F & buff[5]; // this should equal 9 -> 512 bytes for cards <= 1 GB
 
 /*   ; get size into reg 
      ;     6            7         8 
      ; xxxx xxxx    xxxx xxxx    xxxx xxxx 
      ;        ^^    ^^^^ ^^^^    ^^ 
 */ 
 
 data    = (buff[6] & 0x03) << 6; 
 data   |= (buff[7] >> 2); 
 c_size  = data << 4; 
 data    = (buff[7] << 2) | ((buff[8] & 0xC0)>>6); 
 c_size |= data; 
       
      /*   ; get multiplier 
         ;   9         10 
         ; xxxx xxxx    xxxx xxxx 
         ;        ^^    ^ 
      */ 
      
 multi    = ((buff[9] & 0x03 ) << 1); 
 multi   |= ((buff[10] & 0x80) >> 7); 
 sectors  = (c_size + 1) << (multi + 2); 
 
 if(blk_len != 9)                           // sector size > 512B?
    sectors = sectors << (blk_len - 9);     // then capacity of card is bigger
 
 return sectors; 
}
//--------------------------------------------------------
BYTE SpiSD::EraseCard(void)
{
    BYTE res;

    timeoutStart();
 
    if(type != DEVICETYPE_MMC && type != DEVICETYPE_SD && type != DEVICETYPE_SDHC) {   // not a SD/MMC card?
        return 1;
    }

    if(type == DEVICETYPE_MMC)                                  // MMC
        res = mmcSendCommand(MMC_TAG_ERASE_GROUP_START, 0);     // start
    else                                                        // SD or SDHC
        res = mmcSendCommand(MMC_TAG_SECTOR_START, 0);          // start

    if(res!=0) {
        spiCShigh_ff_return(res);    // send 0xff, CS high, return value
    }

    if(type == DEVICETYPE_MMC)        // MMC
        res = mmcSendCommand(MMC_TAG_ERARE_GROUP_END, sdCard.BCapacity - 512);      // end

    if(type == DEVICETYPE_SD)         // SD
        res = mmcSendCommand(MMC_TAG_SECTOR_END, sdCard.BCapacity - 512);           // end

    if(type == DEVICETYPE_SDHC)       // SDHC
        res = mmcSendCommand(MMC_TAG_SECTOR_END, sdCard.SCapacity - 1);             // end

    if(res!=0) {
        spiCShigh_ff_return(res);    // send 0xff, CS high, return value
    }
    //-----------------------------
    // assert chip select
    spiCSlow();                             // CS to L

    res = mmcCommand(MMC_ERASE, 0);         // issue the 'erase' command

    if(res!=0) {                            // if failed
        spiCShigh_ff_return(res);    // send 0xff, CS high, return value
    }

    // TODO: wait while card is busy
    while(spi2_TxRx(0xFF) == 0) {           // wait while the card is busy
    }
    
    spiCShigh_ff_return(0);         // send 0xff, CS high, return value
}

//--------------------------------------------------------
void SpiSD::timeoutStart(void)
{
    opEndTime = Utils::getEndTime(500);
}

bool SpiSD::timeout(void)
{
    DWORD now = Utils::getEndTime(500);
    return (now >= opEndTime);          // it's a timeout when current time is greater than end time
}
//--------------------------------------------------------

