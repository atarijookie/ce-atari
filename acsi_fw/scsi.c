#include "defs.h"
#include "scsi.h"
#include "mmc.h"
#include "bridge.h"

extern BYTE cmd[14];									// received command bytes
extern BYTE cmdLen;										// length of received command
extern BYTE brStat;										// status from bridge

extern TDevice sdCard;
extern BYTE sdCardID;
extern char *VERSION_STRING_SHORT;
extern char *DATE_STRING;

void processScsiLocaly(BYTE justCmd, BYTE isIcd)
{
    BYTE lun;
    
    // get the LUN from the command
    if(isIcd) {                             // for ICD commands
        lun		= cmd[2] >> 5;
    } else {                                // for SCSI6 commands
        lun		= cmd[1] >> 5;					
    }
                
	// The following commands support LUN in command, check if it's valid
	// Note: INQUIRY also supports LUNs, but it should report in a different way...
	if( justCmd == SCSI_C_READ6				|| justCmd == SCSI_C_READ10             || 
        justCmd == SCSI_C_FORMAT_UNIT       || justCmd == SCSI_C_TEST_UNIT_READY    ||
        justCmd == SCSI_C_READ_CAPACITY) {

		if(lun != 0) {					// LUN must be 0
		    scsi_returnLUNnotSupported();
			return;
		}
	}

    // if the card is not init and the command is one of the following
   	if(sdCard.IsInit == FALSE) {
        if( justCmd == SCSI_C_READ6         || justCmd == SCSI_C_READ10     ||
            justCmd == SCSI_C_WRITE6        || justCmd == SCSI_C_WRITE10    || 
            justCmd == SCSI_C_FORMAT_UNIT   || justCmd == SCSI_C_READ_CAPACITY ) {
            
            scsi_returnStatusAccordingToIsInit();
            return;	
        }
    }
    
   	// if media changed, and the command is not INQUIRY and REQUEST SENSE
	if(sdCard.MediaChanged == TRUE) {
		if(justCmd != SCSI_C_INQUIRY && justCmd != SCSI_C_REQUEST_SENSE) {
			scsi_returnUnitAttention();
			return;	
		}
	}

    // if it's a READ / WRITE command
    if( justCmd == SCSI_C_WRITE6    || justCmd == SCSI_C_READ6 || 
        justCmd == SCSI_C_WRITE10   || justCmd == SCSI_C_READ10 ||
        justCmd == SCSI_C_VERIFY ) {
    
        processScsiRW(justCmd, isIcd, lun);
    } else {    // if it's not Read / Write command
        processScsiOther(justCmd, isIcd, lun);
    }
}

void processScsiRW(BYTE justCmd, BYTE isIcd, BYTE lun)
{
    DWORD sector = 0, sectorEnd = 0;
    WORD lenX = 0;
    BYTE res = 0;
    BYTE handled = FALSE;

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
        res = mmcWrite_dma(sector, lenX);
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

void processScsiOther(BYTE justCmd, BYTE isIcd, BYTE lun)
{
    switch(justCmd) {
        case SCSI_C_SEND_DIAGNOSTIC:
        case SCSI_C_RESERVE:
        case SCSI_C_RELEASE:
        case SCSI_C_TEST_UNIT_READY:    scsi_returnStatusAccordingToIsInit();   return;

        case SCSI_C_REQUEST_SENSE:      SCSI_RequestSense();                    return;

        case SCSI_C_FORMAT_UNIT:        SCSI_FormatUnit();                      return;
        
       	case SCSI_C_READ_CAPACITY: 		SCSI_ReadCapacity();                    return;

        case SCSI_C_INQUIRY:            
            if(isIcd == TRUE) {
                ICD7_to_SCSI6();
            }
            
            SCSI_Inquiry(lun);                     
            return;

		//----------------------------------------------------
        default:  
            sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
            sdCard.SCSI_SK      = SCSI_E_IllegalRequest;
            sdCard.SCSI_ASC     = SCSI_ASC_INVALID_COMMAND_OPERATION_CODE;
            sdCard.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

            PIO_read(sdCard.LastStatus);   // send status byte
            break;
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

void scsi_returnStatusAccordingToIsInit(void)
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

void scsi_returnLUNnotSupported(void)
{
    sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
	sdCard.SCSI_SK      = SCSI_E_IllegalRequest;
	sdCard.SCSI_ASC     = SCSI_ASC_LU_NOT_SUPPORTED;
	sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;

    PIO_read(sdCard.LastStatus);                                    // send status byte
}

void scsi_returnUnitAttention(void)
{
	sdCard.MediaChanged = FALSE;
	
	sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
	sdCard.SCSI_SK      = SCSI_E_UnitAttention;
	sdCard.SCSI_ASC     = SCSI_ASC_NOT_READY_TO_READY_TRANSITION;
	sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;

    PIO_read(sdCard.LastStatus);                                    // send status byte
}

void scsi_clearTheUnitAttention(void)
{
	sdCard.LastStatus	= SCSI_ST_OK;
	sdCard.SCSI_SK		= SCSI_E_NoSense;
	sdCard.SCSI_ASC		= SCSI_ASC_NO_ADDITIONAL_SENSE;
	sdCard.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

	sdCard.MediaChanged = FALSE;		
}

void ICD7_to_SCSI6(void)
{
    int i;
    
    for(i=0; i<6; i++) {
        cmd[i] = cmd[i + 1];
    }
}

void SCSI_ReadCapacity(void)
{	 
    // return disk capacity and sector size
    DWORD cap;
    BYTE hi = 0, midlo = 0, midhi = 0, lo = 0;

    ACSI_DATADIR_READ();

    cap = sdCard.SCapacity;
    cap--;

    if(sdCard.IsInit) {
        hi		= (cap >> 24) & 0xff;
        midhi	= (cap >> 16) & 0xff;
        midlo	= (cap >>  8) & 0xff;
        lo		=  cap        & 0xff;
    }

    DMA_read(hi);           // Hi
    DMA_read(midhi);        // mid-Hi
    DMA_read(midlo);        // mid-Lo
    DMA_read(lo);           // Lo

    // return sector size
    DMA_read(0);				 // fixed to 512 B	  
    DMA_read(0);				 
    DMA_read(2);
    DMA_read(0);

    scsi_sendOKstatus();				
}

// return the last error that occured
void SCSI_RequestSense(void)
{
    char i,xx; //, res;
    BYTE val;

    // this command clears the unit attention state
    if(sdCard.MediaChanged == TRUE)	{
        scsi_clearTheUnitAttention();
    }

    xx = cmd[4];		  // how many bytes should be sent

    ACSI_DATADIR_READ();

    for(i=0; i<xx; i++)	{		  
        switch(i) {
            case  0:	val = 0xf0;             break;		// error code 
            case  2:	val = sdCard.SCSI_SK;   break;		// sense key 
            case  7:	val = xx-7;             break;		// AS length
            case 12:	val = sdCard.SCSI_ASC;  break;		// additional sense code
            case 13:	val = sdCard.SCSI_ASCQ; break;		// additional sense code qualifier

            default:	val = 0;                break;
            }
            
        DMA_read(val);
	
        if(brStat != E_OK) {                                // if something isn't OK
            return;                                         // quit this
		}
	}

    scsi_sendOKstatus();
}

void SCSI_Inquiry(BYTE lun)
{
	WORD i,xx;
	BYTE val, firstByte;
	
	BYTE vendor[8]          = {"JOOKIE  "};
    BYTE inquiryName[10]    = {"CosmosEx  "};
	
	if(lun == 0) {						// for LUN 0
		firstByte = 0;
	} else {							// for other LUNs
		firstByte = 0x7f;
	}
	
    // this command clears the unit attention state
    if(sdCard.MediaChanged == TRUE)	{
        scsi_clearTheUnitAttention();
    }
    
	if(cmd[1] & 0x01)                                               // EVPD bit is set? Request for vital data?
	{                                                               // vital data not suported
		sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
		sdCard.SCSI_SK      = SCSI_E_IllegalRequest;
		sdCard.SCSI_ASC     = SCSO_ASC_INVALID_FIELD_IN_CDB;
		sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;

		PIO_read(sdCard.LastStatus);                                // send status byte
		return;	
	}

	//-----------	
	xx = cmd[4];		  									// how many bytes should be sent

	ACSI_DATADIR_READ();

	for(i=0; i<xx; i++)	{		  
		// first init the val to zero or space
		if(i >= 8 && i<=43) {           // if the returned byte is somewhere from ASCII part of data, init on 'space' character
		    val = ' ';
		} else {                        // for other locations init on ZERO
    		val = 0;
		}

		// then for the appropriate position return the right value
		if(i == 0) {					// PERIPHERAL QUALIFIER + PERIPHERAL DEVICE TYPE
			val = firstByte;			// depending on LUN number
		}
		
		if(i==1) {                      // 1st byte
			val = 0x80;                 // removable (RMB) bit set
		}
			
		if(i==2 || i==3) {              // 2nd || 3rd byte
			val = 0x02;                 // SCSI level || response data format
		}			
				
		if(i==4) {
			val = 0x27;                 // 4th byte = Additional length
		}
				
		if(i>=8 && i<=15) {             // send vendor (JOOKIE)
		    val = vendor[i-8];
		}
			
		if(i>=16 && i<=25) {            // send device name (CosmosEx)
		    val = inquiryName[i-16];
		}
		
		if(i == 27) {                   // send ACSI id
			val = '0' + sdCardID;
		}

		if(i>=32 && i<=35) {            // version string
		    val = VERSION_STRING_SHORT[i-32];
		}

		if(i>=36 && i<=43) {            // date string
		    val = DATE_STRING[i-36];
		}
		
		DMA_read(val);	
	
		if(brStat != E_OK) {            // if something isn't OK
			return;                     // quit this
		}
	}	

    scsi_sendOKstatus();
}

void SCSI_FormatUnit(void)
{
    BYTE res = 0;

//	res = EraseCard(devIndex);
	//---------------
	if(res==0) {                                    // if everything was OK
		scsi_sendOKstatus();
    } else {                                        // if error 
		sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
		sdCard.SCSI_SK      = SCSI_E_MediumError;
		sdCard.SCSI_ASC     = SCSI_ASC_NO_ADDITIONAL_SENSE;
		sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;

		PIO_read(sdCard.LastStatus);                            // send status byte
	}
}
