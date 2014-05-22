#include "defs.h"
#include "scsi.h"
#include "mmc.h"
#include "bridge.h"

extern BYTE cmd[14];									// received command bytes
extern BYTE cmdLen;										// length of received command
extern BYTE brStat;										// status from bridge

extern TDevice sdCard;

void processScsiLocaly(BYTE justCmd)
{
    DWORD sector = 0, sectorEnd = 0;
    WORD lenX = 0;
    BYTE res = 0;
    BYTE handled = FALSE;

    // if the card is not init
   	if(sdCard.IsInit != TRUE) {
        returnStatusAccordingToIsInit();
        return;	
    }
    
    // if it's 6 byte RW command
    if(justCmd == SCSI_C_WRITE6 || justCmd == SCSI_C_READ6) {
        sector  = (cmd[1] & 0x1f);      // get the starting sector
        sector  = sector << 8;
        sector |= cmd[2];
        sector  = sector << 8;
        sector |= cmd[3];

        lenX = cmd[4];	   	 	   	    // get the # of sectors to read
    }

    // if it's 10 byte RW command
    if( justCmd == SCSI_C_WRITE10 || justCmd == SCSI_C_READ10 || 
        justCmd == SCSI_C_VERIFY) {
        sector  = cmd[3];               // get the starting sector
        sector  = sector << 8;
        sector |= cmd[4];
        sector  = sector << 8;
        sector |= cmd[5];
        sector  = sector << 8;
        sector |= cmd[6];

        lenX  = cmd[8];	  	   		    // get the # of sectors to read
        lenX  = lenX << 8;
        lenX |= cmd[9];
    }

    if(lenX != 0) {                     // for read / write / verify with length of more than 0 sectors
        sectorEnd = sector + ((DWORD)lenX) - 1;                             // calculate ending sector

        if( sector >= sdCard.SCapacity || sectorEnd >= sdCard.SCapacity ) { // are we out of range?
            sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
            sdCard.SCSI_SK      = SCSI_E_IllegalRequest;
            
            PIO_read(sdCard.LastStatus);                                    // return error
            return;
        }
    }
    
    // for sector read commands
    if(justCmd == SCSI_C_READ6 || justCmd == SCSI_C_READ10) {
        res = mmcRead_dma(sector, lenX);                                    // read data
        
        handled = TRUE;
    }
    
    // for sector write commands
    if(justCmd == SCSI_C_WRITE6 || justCmd == SCSI_C_WRITE6) {
        if(lenX == 1) {
            res = mmcWrite(sector);
        } else {
            res = mmcWriteMore(sector, lenX);
        }

        handled = TRUE;
    }
    
    // return status for read and write command
    if(handled) {                                                   // if it was handled before - read and write command
        if(res==0) {                                                // if everything was OK
            scsi_sendOKstatus();
        } else {                                                    // if error 
            sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
            sdCard.SCSI_SK      = SCSI_E_MediumError;

            PIO_read(sdCard.LastStatus);                            // send status byte, long time-out
        }
    }
    
    // verify command handling
    if(justCmd == SCSI_C_VERIFY) {
        res = mmcCompareMore(sector, lenX);                         // compare sectors
        
        if(res != 0) {
   			sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
			sdCard.SCSI_SK      = SCSI_E_Miscompare;
		
			PIO_read(sdCard.LastStatus);                            // send status byte
        } else {
            scsi_sendOKstatus();
        }
    }
}

void scsi_sendOKstatus(void)
{
	sdCard.LastStatus   = SCSI_ST_OK;
	sdCard.SCSI_SK      = SCSI_E_NoSense;

	PIO_read(sdCard.LastStatus);                                    // send status byte, long time-out 
}

void returnStatusAccordingToIsInit(void)
{
    if(sdCard.IsInit == TRUE) {                                     // SD card is init?
        scsi_sendOKstatus();
    } else {                                                        // SD card not init?
        sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
        sdCard.SCSI_SK      = SCSI_E_NotReady;

        PIO_read(sdCard.LastStatus);                                // send status byte
    }
}
