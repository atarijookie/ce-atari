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
    DWORD sector;
    WORD lenX;
    BYTE res;
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
    
    // for sector read commands
    if(justCmd == SCSI_C_READ6 || justCmd == SCSI_C_READ10) {
        if(lenX == 1) {
            res = mmcRead(sector);
        } else {
            res = mmcReadMore(sector, lenX);
        }
        
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
            sdCard.SCSI_ASC     = SCSI_ASC_NO_ADDITIONAL_SENSE;
            sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;

            PIO_read(sdCard.LastStatus);                            // send status byte, long time-out
        }
    }
    
    // verify command handling
    if(justCmd == SCSI_C_VERIFY) {
        res = mmcCompareMore(sector, lenX);                         // compare sectors
        
        if(res != 0) {
   			sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
			sdCard.SCSI_SK      = SCSI_E_Miscompare;
			sdCard.SCSI_ASC     = SCSI_ASC_VERIFY_MISCOMPARE;
			sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;
		
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
	sdCard.SCSI_ASC     = SCSI_ASC_NO_ADDITIONAL_SENSE;
	sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;

	PIO_read(sdCard.LastStatus);                                    // send status byte, long time-out 
}

void returnStatusAccordingToIsInit(void)
{
    if(sdCard.IsInit == TRUE) {                                     // SD card is init?
        scsi_sendOKstatus();
    } else {                                                        // SD card not init?
        sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
        sdCard.SCSI_SK      = SCSI_E_NotReady;
        sdCard.SCSI_ASC     = SCSI_ASC_MEDIUM_NOT_PRESENT;
        sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        PIO_read(sdCard.LastStatus);                                // send status byte
    }
}
