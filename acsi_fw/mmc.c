#include "stm32f10x_spi.h"

#include "mmc.h"
#include "defs.h"
#include "bridge.h"

TDevice sdCard;

extern unsigned char brStat;

void waitForSPI2idle(void);

BYTE isCardInserted(void)
{
    WORD inputs = GPIOC->IDR;
    
    if((inputs & SD_DETECT) == 0) {
        return TRUE;
    }
    
    return FALSE;
}

void spiSetFrequency(BYTE highNotLow)
{
    WORD tmpreg = 0;
  
    tmpreg = SPI2->CR1;
    tmpreg &= ~(0x0038);                            // clear bits 3,4,5 (BR)
  
    if(highNotLow == SPI_FREQ_HIGH) {               // high frequency? 18 MHz (72 / 4)
        tmpreg |= SPI_BaudRatePrescaler_4;
    } else {                                        // low frequency? 0.56 MHz (72 / 128)
        tmpreg |= SPI_BaudRatePrescaler_128;
    }

    SPI2->CR1 = tmpreg;                             // set new baudrate
}

void sdCardZeroInitStruct(void)
{
    sdCard.IsInit       = FALSE;                                            
    sdCard.Type         = DEVICETYPE_NOTHING;
    sdCard.MediaChanged = FALSE;
    sdCard.BCapacity    = 0;
	sdCard.SCapacity    = 0;
}

void sdCardInit(void)
{
    WORD i;
    BYTE res, stat;
    DWORD dRes;
    
    //--------------------------
    // remove all not present devices
    if(isCardInserted() == FALSE) {             // card not present, then remove
        sdCardZeroInitStruct();        
        return;
    }

    //--------------------------
    // and now try to init all that is not initialized
    spiSetFrequency(SPI_FREQ_LOW);              // low SPI frequency

    if(sdCard.IsInit == TRUE)                   // initialized?
        return;                                 // skip it!

    for(i=0; i<5; i++) {
        res = isCardInserted();                 // is it inserted?
                
        if(res == FALSE) {                      // if not inserted
            return;
        }

        res = mmcReset();                       // init the MMC card
                
        if(res == DEVICETYPE_NOTHING) {         // if failed to init
            continue;
        }

        // now to test if we can read from the card
        stat = mmcReadJustForTest(0);        

        if(stat) {
            continue;
        }                       

        // get card capacity in blocks 
        if(res == DEVICETYPE_SDHC) {
            dRes = SDHC_Capacity();
        } else {
            dRes = MMC_Capacity();
        }
                
        if(dRes == 0xffffffff) {                // failed?
            // wait 50 ms
            continue;
        }
        
        sdCard.Type         = res;              // store device type
        sdCard.SCapacity    = dRes;             // store capacity in sectors
        sdCard.BCapacity    = dRes << 9;        // store capacity in bytes
        sdCard.IsInit       = TRUE;             // set the flag
                
        sdCard.MediaChanged = TRUE;
        
        tryToEmptySdCardBuffer();               // try to empty the internal buffer of SD card
        
        break;
    }
    
    spiSetFrequency(SPI_FREQ_HIGH); 
}
//-----------------------------------------------
// It seems that on some cards the init sequence (or something from it) might cause the card
// to want to talk a little bit more than expected, and thus the 1st command issued to this 
// specific card might fail. For this situation just sending FFs and reading back the results
// might cure the situation. (Happened to me on 1 out of 5 cards, other 4 were working fine). 

void tryToEmptySdCardBuffer(void)
{
    WORD j;
    
    spi2_TxRx(0xFF);
    spi2_TxRx(0xFF);
    waitForSPI2idle();
    spiCSlow();
    
    for(j=0; j<1024; j++) {
        spi2_TxRx(0xFF);
    }
    waitForSPI2idle();

    spiCShigh();
    
    spi2_TxRx(0xFF);
    spi2_TxRx(0xFF);
    waitForSPI2idle();
}

BYTE mmcReset(void)
{
    BYTE r1;
    WORD retry;
    BYTE isSD, buff[5];
    
    timeoutStart();
    
    spiSetFrequency(SPI_FREQ_LOW);
    spiCShigh();
    
    // send dummy bytes with CS high before accessing
    for(r1=0; r1<10; r1++) {
        spi2_TxRx(0xFF);
    }

    //---------------
    // now send the card to IDLE state  
    r1 = mmcSendCommand(MMC_GO_IDLE_STATE, 0);
    
    if(r1 != 1)
        return DEVICETYPE_NOTHING;
    //---------------
    isSD    = FALSE;
    
    r1 = mmcSendCommand5B(SDHC_SEND_IF_COND, 0x1AA, buff);         // init SDHC card
    
    if(r1 == 0x01)              // if got the right response -> it's an SD or SDHC card
    {
        if(buff[3] == 0x01 && buff[4] == 0xaa)      // if the rest of R7 is OK (OK echo and voltage)
        {
            //--------------
            retry = 0xffff;
    
            while(retry)
            {
                retry--;
                
                if(timeout()) {
                    retry = 0;
                    break;
                }
                
                r1 = mmcSendCommand(55, 0);         // ACMD41 = CMD55 + CMD41

                if(r1 != 1)                         // if invalid reply to CMD55, then it's MMC card
                {
                    retry = 0;                      // fuck, it's MMC card
                    break;          
                }
        
                r1 = mmcSendCommand(41, 0x40000000); // ACMD41 = CMD55 + CMD41 ---  HCS (High Capacity Support) bit set

                if(r1 == 0)                         // if everything is OK, then it's SD card
                    break;
                    
                if(timeout()) {
                    retry = 0;
                    break;
                }
            }
            //--------------
            if(retry)                               // if not timed out
            {
                r1 = mmcSendCommand5B(MMC_READ_OCR, 0, buff);          

                if(r1 == 0)                         // if command succeeded
                {
                    if(buff[1] & 0x40) {            // if CCS bit is set -> SDHC card, else SD card
                        return DEVICETYPE_SDHC;
                    } else {
                        isSD    = TRUE;
                    }
                }
            }           
        }       
    }
    //---------------
    // if we came here, then it's initialized SD card or not initialized MMC card
    if(isSD == FALSE) {                             // it's not an initialized SD card, try to init
        retry = 0xffff;
    } else {
        retry = 0;                                  // if it's initialized SD card, skip init
        r1 = 0;
    }
    //---------------
    while(retry) {
        r1 = mmcSendCommand(55, 0);                 // ACMD41 = CMD55 + CMD41
        
        if(r1 != 1) {                               // if invalid reply to CMD55, then it's MMC card
            break;   
        }
        
        r1 = mmcSendCommand(41, 0);                 // ACMD41 = CMD55 + CMD41
        
        if(r1 == 0) {                               // if everything is OK, then it's SD card
            isSD = TRUE;
            break;
        }
        
        if(timeout()) {
            retry = 0;
            break;
        }
        
        retry--;
    }
    
    if(isSD && r1!=0) {                             // if it's SD but failed to initialize
        return DEVICETYPE_NOTHING;
    }
        
    //-------------------------------
    if(isSD==FALSE) {
        // try to initialize the MMC card
        r1 = mmcCmdLow(MMC_SEND_OP_COND, 0, 0);
    
        if(r1 != 0) {
            return DEVICETYPE_NOTHING;
        }
    }
    
    // set block length to 512 bytes
    r1 = mmcSendCommand(MMC_SET_BLOCKLEN, 512);
        
    if(isSD==TRUE) {
        return DEVICETYPE_SD;
    } else {
        return DEVICETYPE_MMC;
    }
}
//-----------------------------------------------
BYTE mmcCmd(BYTE cmd, DWORD arg, BYTE retry, BYTE val)
{
    BYTE r1;

    do {
        r1 = mmcSendCommand(cmd, arg);
              
        if(retry==0 || timeout())
            return 0xff;

        // do retry counter
        retry--;
        
    } while(r1 != val);
    
    return r1;
}
//-----------------------------------------------
BYTE mmcCmdLow(BYTE cmd, DWORD arg, BYTE val)
{
    BYTE r1;
    WORD retry = 0xffff;
     
    spiCSlow();          // CS to L
    
    do
    {
        // issue the command
        r1 = mmcSendCommand(cmd, arg);

        spi2_TxRx(0xff);

        if(retry==0 || timeout()) {
        
            for(retry=0; retry<10; retry++)
                spi2_TxRx(0xff);

            spiCShigh();         // CS to H
            return 0xff;
        }

        // do retry counter
        retry--;
            
    } while(r1 != val);

    for(retry=0; retry<10; retry++)
        spi2_TxRx(0xff);
    
    spiCShigh();         // CS to H
    
    return r1;
}
//-----------------------------------------------
BYTE mmcSendCommand(BYTE cmd, DWORD arg)
{
    BYTE r1;

    // assert chip select
    spiCSlow();          // CS to L

    spi2_TxRx(0xFF);

    // issue the command
    r1 = mmcCommand(cmd, arg);

    spi2_TxRx(0xFF);

    // release chip select
    spiCShigh();         // CS to H

    return r1;
}
//-----------------------------------------------
BYTE mmcSendCommand5B(BYTE cmd, DWORD arg, BYTE *buff)
{
    BYTE r1, i;

    // assert chip select
    spiCSlow();          // CS to L

    spi2_TxRx(0xFF);

    // issue the command
    r1 = mmcCommand(cmd, arg);
    buff[0] = r1;
    
    // receive the rest of R7 register  
    for(i=1; i<5; i++)                                      
        buff[i] = spi2_TxRx(0xFF);
        
    spi2_TxRx(0xFF);

    // release chip select
    spiCShigh();         // CS to H

    return r1;
}
//-----------------------------------------------
BYTE mmcCommand(BYTE cmd, DWORD arg)
{
    BYTE r1;
    WORD retry=0xffff;              // v. 1.00
//  DWORD retry=0x001fffff;         // experimental

    spi2_TxRx(0xFF);

    // send command
    spi2_TxRx(cmd | 0x40);
    spi2_TxRx(arg>>24);
    spi2_TxRx(arg>>16);
    spi2_TxRx(arg>>8);
    spi2_TxRx(arg);
    
    if(cmd == SDHC_SEND_IF_COND)
        spi2_TxRx(0x86);              // crc valid only for SDHC_SEND_IF_COND
    else
        spi2_TxRx(0x95);              // crc valid only for MMC_GO_IDLE_STATE
    
    // end command
    // wait for response
    // if more than 8 retries, card has timed-out
    // return the received 0xFF
    while(1)
    {
        r1 = spi2_TxRx(0xFF);

        if(r1 != 0xff)
            break;
        
        if(retry == 0 || timeout())
            break;
            
        retry--;
    }
    
    // return response
    return r1;
}
//**********************************************************************
BYTE mmcRead(DWORD sector)
{
    BYTE r1;
    DWORD i;
    BYTE byte;
    BYTE failed = 0;

    timeoutStart();
    
    // assert chip select
    spiCSlow();          // CS to L

    if(sdCard.Type != DEVICETYPE_SDHC)                  // for non SDHC cards change sector into address
        sector = sector<<9;

    // issue command
    r1 = mmcCommand(MMC_READ_SINGLE_BLOCK, sector);

    // check for valid response
    if(r1 != 0x00)
    {
        spi2_TxRx(0xFF);
        spiCShigh();         // CS to H

        return r1;
    }

    //------------------------
    // wait for block start
    i = 0x000fffff;             // v. 1.00
    
    while(i)
    {
        r1 = spi2_TxRx(0xFF);                            // get byte

        if(r1 == MMC_STARTBLOCK_READ)                   // if it's the wanted byte
            break;
        
        if(timeout()) {
            i = 0;
            break;
        }
        
        i--;                                          // decrement
    }
    //------------------------
    if(i == 0)                                        // timeout?
    {
        spi2_TxRx(0xFF);
        spiCShigh();         // CS to H

        return 0xff;
    }
    //------------------------
    // read in data
    ACSI_DATADIR_READ();

    spi2_Tx(0xff);                                      // start the SPI transfer

    for(i=0; i<0x200; i++) {
//        byte = spi2_TxRx(0xFF);                       // get byte
        byte = spi2_Rx();                               // receive byte from previous TX part
        spi2_Tx(0xff);                                  // and start another TX part
        
        DMA_read(byte);                                 // send it to ST

        if(brStat != E_OK) {                            // if something was wrong
            byte = spi2_Rx();                           // dummy read, because there was one more TX than needed
            failed = 1;
            
            for(; i<0x200; i++)                         // finish the sector
                spi2_TxRx(0xFF);
          
            break;                                      // quit
        }
    }

    if(failed == 0) {
        byte = spi2_Rx();                               // dummy read, because there was one more TX than needed
    }
        
    // read 16-bit CRC
    spi2_TxRx(0xFF);                  // get byte
    spi2_TxRx(0xFF);                  // get byte

    spi2_TxRx(0xFF);

    spiCShigh();         // CS to H
///////////////////////////////////
// !!! THIS HELPS TO SYNCHRONIZE THE THING !!!
    for(i=0; i<3; i++)                  
        spi2_TxRx(0xFF);
///////////////////////////////////

    if(brStat != E_OK)
    {
        return 0xff;
    }
        
    //-----------------
    // return success
    return 0;   
}
//-----------------------------------------------
BYTE mmcReadMore(DWORD sector, WORD count)
{
    BYTE r1, quit;
    WORD i,j;
    BYTE byte;

    timeoutStart();
    
    // assert chip select
    spiCSlow();                                             // CS to L

    if(sdCard.Type != DEVICETYPE_SDHC)                      // for non SDHC cards change sector into address
        sector = sector<<9;

    // issue command
    r1 = mmcCommand(MMC_READ_MULTIPLE_BLOCKS, sector);

    // check for valid response
    if(r1 != 0x00) {
        spi2_TxRx(0xFF);
        spiCShigh();                                        // CS to H
        return r1;
    }

    // read in data
    ACSI_DATADIR_READ();
    
    quit = 0;
    
    for(j=0; j<count; j++)                                  // read this many sectors
    {
        // wait for block start
        while(spi2_TxRx(0xFF) != MMC_STARTBLOCK_READ);

        spi2_Tx(0xff);                                      // start the SPI transfer

        for(i=0; i<0x200; i++)                              // read this many bytes
        {
//            byte = spi2_TxRx(0xFF);                          // get byte
            byte = spi2_Rx();                               // receive byte from previous TX part
            spi2_Tx(0xff);                                  // and start another TX part

            DMA_read(byte);                                 // send it to ST
        
            if(brStat != E_OK) {                            // if something was wrong
                byte = spi2_Rx();                           // read the extra SPI byte
                
                for(; i<0x200; i++)                         // finish the sector
                spi2_TxRx(0xFF);

                quit = 1;
                break;                                      // quit
            }
        }           
        
        if(quit != 1) {
            byte = spi2_Rx();                               // read the extra SPI byte
        }
        //---------------
        if((count - j) == 1)                                // if we've read the last sector
            break;
        
        if(quit)                                            // if error happened
            break;      
            
        // if we need to read more, then just read 16-bit CRC
        spi2_TxRx(0xFF);
        spi2_TxRx(0xFF);
    }
    //-------------------------------
    // stop the transmition of next sector
    mmcCommand(MMC_STOP_TRANSMISSION, 0);               // send command instead of CRC
    spi2_TxRx(0xFF);
    spiCShigh();                                        // CS to H

    if(quit) {                                              // if error happened
        return 0xff;
    }   
    
    return 0;   
}
//-----------------------------------------------
BYTE mmcWriteMore(DWORD sector, WORD count)
{
    BYTE r1, quit;
    WORD i,j;
    DWORD thisSector;

    timeoutStart();
    
    // assert chip select
    spiCSlow();          // CS to L

    thisSector = sector;
    
    if(sdCard.Type != DEVICETYPE_SDHC)          // for non SDHC cards change sector into address
        sector = sector<<9;

    //--------------------------
/*  
    // for SD and SDHC cards issue an SET_WR_BLK_ERASE_COUNT command
    if(sdCard.Type == DEVICETYPE_SDHC || sdCard.Type == DEVICETYPE_SD)      
    {
        mmcCommand(55, 0);                      // ACMD23 = CMD55 + CMD23
        mmcCommand(23, (DWORD) count);  
    }       
*/
    //--------------------------
    // issue command
    r1 = mmcCommand(MMC_WRITE_MULTIPLE_BLOCKS, sector);

    // check for valid response
    if(r1 != 0x00)
    {
        spi2_TxRx(0xFF);
        spiCShigh();                            // CS to H
        return r1;
    }
    
    //--------------
    // read in data
    ACSI_DATADIR_WRITE();
    
    quit = 0;
    //--------------
    for(j=0; j<count; j++)                      // read this many sectors
    {
        if(thisSector >= sdCard.SCapacity)      // sector out of range?
        {
            quit = 1;
            break;                              // quit
        }
        //--------------            
        while(spi2_TxRx(0xFF) != 0xff);          // while busy
        
        spi2_TxRx(MMC_STARTBLOCK_MWRITE);        // 0xfc as start write multiple blocks
        //--------------            
        
        for(i=0; i<0x200; i++)                  // read this many bytes
        {
            r1 = DMA_write();                   // get it from ST

            if(brStat != E_OK)                  // if something was wrong
            {
                for(; i<0x200; i++)             // finish the sector
                spi2_TxRx(0xFF);

                quit = 1;
                break;                          // quit
            }
            
            spi2_TxRx(r1);                       // send it to card
        }           
        
        // send more: 16-bit CRC
        spi2_TxRx(0xFF);
        spi2_TxRx(0xFF);
        
        thisSector++;                           // increment real sector #
        //---------------
        if(quit)                                // if error happened
            break;      
    }
    //-------------------------------
    while(spi2_TxRx(0xFF) != 0xff);              // while busy

    spi2_TxRx(MMC_STOPTRAN_WRITE);               // 0xfd to stop write multiple blocks

    while(spi2_TxRx(0xFF) != 0xff);              // while busy
    //-------------------------------
    // for the MMC cards send the STOP TRANSMISSION also
    if(sdCard.Type == DEVICETYPE_MMC)       
    {
        mmcCommand(MMC_STOP_TRANSMISSION, 0);

        while(spi2_TxRx(0xFF) != 0xff);          // while busy
    }
    //-------------------------------
    spi2_TxRx(0xFF);
    spiCShigh();                                // CS to H
    
    if(quit)                                    // if failed, return error
    {
        return 0xff;
    }
        
    return 0;                                   // success
}
//-----------------------------------------------
BYTE mmcReadJustForTest(DWORD sector)
{
    BYTE r1;
    DWORD i;

    timeoutStart();
    
    // assert chip select
    spiCSlow();          // CS to L

    if(sdCard.Type != DEVICETYPE_SDHC)          // for non SDHC cards change sector into address
        sector = sector<<9;

    // issue command
    r1 = mmcCommand(MMC_READ_SINGLE_BLOCK, sector);

    // check for valid response
    if(r1 != 0x00)
    {
        spi2_TxRx(0xFF);
        spiCShigh();                            // CS to H

        return r1;
    }

    //------------------------
    // wait for block start
    i = 0x000fffff;
    
    while(i != 0)
    {
        r1 = spi2_TxRx(0xFF);                    // get byte

        if(r1 == MMC_STARTBLOCK_READ)           // if it's the wanted byte
            break;
        
        if(timeout()) {
            i = 0;
            break;
        }
        
        i--;                                    // decrement
    }
    //------------------------
    if(i == 0)                                  // timeout?
    {
        spi2_TxRx(0xFF);
        spiCShigh();                            // CS to H

        return 0xff;
    }
    //------------------------
    // read in data
    for(i=0; i<0x200; i++) {
        BYTE val = spi2_TxRx(0xFF);                  // get byte
    }

    // read 16-bit CRC
    spi2_TxRx(0xFF);                             // get byte
    spi2_TxRx(0xFF);                             // get byte

    spi2_TxRx(0xFF);

    spiCShigh();                                // CS to H

    // return success
    return 0;   
}
//-----------------------------------------------
BYTE mmcWrite(DWORD sector)
{
    BYTE r1;
    DWORD i;
    
    timeoutStart();
    
    //-----------------
    // assert chip select
    spiCSlow();                                 // CS to L
    
    if(sdCard.Type != DEVICETYPE_SDHC)          // for non SDHC cards change sector into address
        sector = sector<<9;
        
    // issue command
    r1 = mmcCommand(MMC_WRITE_BLOCK, sector);

    // check for valid response
    if(r1 != 0x00)
    {
        spi2_TxRx(0xFF);
        spiCShigh();                            // CS to H

        return r1;
    }
    
    // send dummy
    spi2_TxRx(0xFF);
    
    // send data start token
    spi2_TxRx(MMC_STARTBLOCK_WRITE);
    // write data

    ACSI_DATADIR_WRITE();
//==================================================
    for(i=0; i<0x200; i++)
    {
        r1 = DMA_write();                       // get it from ST

        if(brStat != E_OK)                      // if something was wrong
        {
            for(; i<0x200; i++)                 // finish the sector
                spi2_TxRx(0);
    
            break;                              // quit
        }  

        spi2_TxRx(r1);                           // send byte
    }
    
//==================================================
        
    // write 16-bit CRC (dummy values)
    spi2_TxRx(0xFF);
    spi2_TxRx(0xFF);
    
    // read data response token
    r1 = spi2_TxRx(0xFF);
    
    if( (r1&MMC_DR_MASK) != MMC_DR_ACCEPT)
    {
        spi2_TxRx(0xFF);
        spiCShigh();                            // CS to H
        return r1;
    }

    // wait until card not busy
    //------------------------
    // wait for block start
    i = 0x000fffff;
    
    while(i != 0)
    {
        r1 = spi2_TxRx(0xFF);                    // receive byte
        
        if(r1 != 0)                             // if it's the wanted byte
            break;
        
        if(timeout()) {
            i = 0;
            break;
        }
            
        i--;                                    // decrement
    }
    //------------------------
    spi2_TxRx(0xFF);
    spiCShigh();                                // CS to H

    if(i == 0) {                                // timeout?
        return 0xff;
    }
    
    return 0;                                   // return success
}
//--------------------------------------------------------
/** 
*   Retrieves the CSD Register from the mmc 
* 
*   @return      Status response from cmd 
**/ 
BYTE MMC_CardType(unsigned char *buff) 
{ 
 BYTE byte, i; 
 
 // assert chip select
    spiCSlow();                     // CS to L

 // issue the command
 byte = mmcCommand(MMC_SEND_CSD, 0);

 if (byte!=0)                       // if error 
 {
    spiCShigh();                    // CS to H
    spi2_TxRx(0xFF);                 // Clear SPI 
    
    return byte;
 } 

 // wait for block start
 for(i=0; i<16; i++)
    {
    byte = spi2_TxRx(0xFF);
     
    if(byte == MMC_STARTBLOCK_READ)
        break;
    }

 if (byte!=MMC_STARTBLOCK_READ)     // if error 
 {
    spiCShigh();                    // CS to H
    spi2_TxRx(0xFF);                 // Clear SPI 
    
    return byte;
 } 

 // read the data
 for (i=0; i<16; i++)
     buff[i] = spi2_TxRx(0xFF); 

 spiCShigh();                       // CS to H
 spi2_TxRx(0xFF);                    // Clear SPI 
    
 return 0; 
} 
//--------------------------------------------------------
DWORD SDHC_Capacity(void) 
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
DWORD MMC_Capacity(void) 
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
BYTE EraseCard(void)
{
    BYTE res;

    timeoutStart();
 
    if(sdCard.Type != DEVICETYPE_MMC && sdCard.Type != DEVICETYPE_SD && sdCard.Type != DEVICETYPE_SDHC) {   // not a SD/MMC card?
        return 1;
    }

    if(sdCard.Type == DEVICETYPE_MMC)                           // MMC
        res = mmcSendCommand(MMC_TAG_ERASE_GROUP_START, 0);     // start
    else                                                        // SD or SDHC
        res = mmcSendCommand(MMC_TAG_SECTOR_START, 0);          // start

    if(res!=0) {
        spi2_TxRx(0xFF);
        spiCShigh();                        // CS to H

        return res;
    }

    if(sdCard.Type == DEVICETYPE_MMC)        // MMC
        res = mmcSendCommand(MMC_TAG_ERARE_GROUP_END, sdCard.BCapacity - 512);      // end

    if(sdCard.Type == DEVICETYPE_SD)         // SD
        res = mmcSendCommand(MMC_TAG_SECTOR_END, sdCard.BCapacity - 512);           // end

    if(sdCard.Type == DEVICETYPE_SDHC)       // SDHC
        res = mmcSendCommand(MMC_TAG_SECTOR_END, sdCard.SCapacity - 1);             // end

    if(res!=0) {
        spi2_TxRx(0xFF);
        spiCShigh();                        // CS to H

        return res;
    }
    //-----------------------------
    // assert chip select
    spiCSlow();                             // CS to L

    res = mmcCommand(MMC_ERASE, 0);         // issue the 'erase' command

    if(res!=0)                              // if failed
    {
        spi2_TxRx(0xFF);
        spiCShigh();                        // CS to H
        return res;
    }

    while(spi2_TxRx(0xFF) == 0) {           // wait while the card is busy
    }
    
    spiCShigh();                            // CS to H

    spi2_TxRx(0xFF);
    return 0;
}
//--------------------------------------------------------
//////////////////////////////////////////////////////////////////
BYTE mmcCompareMore(DWORD sector, WORD count)
{
    WORD i;
    BYTE res;
    
    for(i=0; i<count; i++) {                // go through all required sectors
        res = mmcCompare(sector + i);       // compare them
        
        if(res != 0) {                      // compare didn't go well?
            return 0xff;
        }
    }
    
    return 0;
}

BYTE mmcCompare(DWORD sector)
{
    BYTE r1;
    DWORD i;
    BYTE byteSD, byteST;

    timeoutStart();
    
    // assert chip select
    spiCSlow();                             // CS to L

    if(sdCard.Type != DEVICETYPE_SDHC)  // for non SDHC cards change sector into address
        sector = sector<<9;

    // issue command
    r1 = mmcCommand(MMC_READ_SINGLE_BLOCK, sector);

    // check for valid response
    if(r1 != 0x00)
    {
        spi2_TxRx(0xFF);
        spiCShigh();                        // CS to H

        return r1;
    }

    //------------------------
    // wait for block start
    i = 0x000fffff;             
    
    while(i)
    {
        r1 = spi2_TxRx(0xFF);                // get byte

        if(r1 == MMC_STARTBLOCK_READ)       // if it's the wanted byte
            break;
            
        if(timeout()) {
            i = 0;
            break;
        }       
            
        i--;                                // decrement
    }
    //------------------------
    if(i == 0)                              // timeout?
    {
        spi2_TxRx(0xFF);
        spiCShigh();                        // CS to H

        return 0xff;
    }
    //------------------------
    // read in data

    ACSI_DATADIR_WRITE();

    for(i=0; i<0x200; i++)
    {
        byteSD = spi2_TxRx(0xFF);                        // get byte from card

        byteST = DMA_write();                           // get byte from Atari

        if((brStat != E_OK) || (byteST != byteSD))      // if something was wrong
        {
            for(; i<0x200; i++)                         // finish the sector
                spi2_TxRx(0xFF);
          
            break;                                      // quit
        }
    }

    // read 16-bit CRC
    spi2_TxRx(0xFF);                                     // get byte
    spi2_TxRx(0xFF);                                     // get byte

    spi2_TxRx(0xFF);

    spiCShigh();                                        // CS to H
///////////////////////////////////
// !!! THIS HELPS TO SYNCHRONIZE THE THING !!!
    for(i=0; i<3; i++)                  
        spi2_TxRx(0xFF);
///////////////////////////////////

    if(brStat != E_OK) {
        return 0xff;
    }
        
    //-----------------
    // return success
    return 0;   
}
//-----------------------------------------------
WORD spi2_TxRx(WORD out)
{
    WORD in;

    while((SPI2->SR & (1 << 7)) != 0);                      // TXE flag: BUSY flag

    while((SPI2->SR & 2) == 0);                             // TXE flag: Tx buffer empty
    SPI2->DR = out;                                         // send over SPI

    while((SPI2->SR & 1) == 0);                             // RXNE flag: RX buffer NOT empty
    in = SPI2->DR;                                          // get data
    
    return in;
}
//-----------------------------------------------
void spi2_Tx(WORD out)
{
    while((SPI2->SR & (1 << 7)) != 0);                      // TXE flag: BUSY flag

    while((SPI2->SR & 2) == 0);                             // TXE flag: Tx buffer empty
    SPI2->DR = out;                                         // send over SPI
}
//-----------------------------------------------
WORD spi2_Rx(void)
{
    WORD in;

    while((SPI2->SR & 1) == 0);                             // RXNE flag: RX buffer NOT empty
    in = SPI2->DR;                                          // get data
    
    return in;
}
//-----------------------------------------------
