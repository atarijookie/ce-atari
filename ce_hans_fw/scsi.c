#include "defs.h"
#include "scsi.h"
#include "mmc.h"
#include "bridge.h"
#include "eeprom.h"
#include "timers.h"

extern BYTE cmd[14];									// received command bytes
extern BYTE cmdLen;										// length of received command
extern BYTE brStat;										// status from bridge
extern BYTE pioReadFailed;

WORD sdErrorCountWrite;                     // counter of failed SD writes
WORD sdErrorCountRead;                      // counter of failed SD reads

extern TDevice sdCard;
extern BYTE sdCardID;
extern char *VERSION_STRING_SHORT;
extern char *DATE_STRING;

extern BYTE firstConfigReceived;

ScsiLogItem scsiLogs[SCSI_LOG_LENGTH];
BYTE scsiLogNow;

void memcpy(char *dest, char *src, int cnt);

void processScsiLocaly(BYTE justCmd, BYTE isIcd)
{
    BYTE lun;

    timeoutStart();                         // start the timeout timer to give the rest of code full timeout time

    // get the LUN from the command
    if(isIcd) {                             // for ICD commands
        lun		= cmd[2] >> 5;
    } else {                                // for SCSI6 commands
        lun		= cmd[1] >> 5;
    }

    // if LUN is not zero, reject command
    if(lun != 0) {                          // LUN must be 0, fail immediatelly for all commands except REQUST SENSE and INQUIRY
        if(justCmd != SCSI_C_REQUEST_SENSE && justCmd != SCSI_C_INQUIRY) {
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
    DWORD sectorStart = 0, sectorEnd = 0;
    WORD  sectorCount = 0;
    BYTE res = 0;
    BYTE handled    = FALSE;
    BYTE wasRead    = FALSE;
    BYTE wasWrite   = FALSE;

    // if it's 6 byte RW command
    if(justCmd == SCSI_C_WRITE6 || justCmd == SCSI_C_READ6) {
        sectorStart  = (cmd[1] & 0x1f);     // get the starting sector
        sectorStart  = sectorStart << 8;
        sectorStart |= cmd[2];
        sectorStart  = sectorStart << 8;
        sectorStart |= cmd[3];

        sectorCount = cmd[4];               // get the # of sectors to read

        if(sectorCount == 0) {              // if sector count is zero, it's 256 sectors (shouldn't happen on ST, could happen on TT / Falcon)
            sectorCount = 256;
        }
    }

    // if it's 10 byte RW command
    if( justCmd == SCSI_C_WRITE10 || justCmd == SCSI_C_READ10 || justCmd == SCSI_C_VERIFY) {
        sectorStart  = cmd[3];              // get the starting sector
        sectorStart  = sectorStart << 8;
        sectorStart |= cmd[4];
        sectorStart  = sectorStart << 8;
        sectorStart |= cmd[5];
        sectorStart  = sectorStart << 8;
        sectorStart |= cmd[6];

        sectorCount  = cmd[8];              // get the # of sectors to read
        sectorCount  = sectorCount << 8;
        sectorCount |= cmd[9];
    }

    if(sectorCount != 0) {                     // for read / write / verify with length of more than 0 sectors
        sectorEnd = sectorStart + ((DWORD)sectorCount) - 1;         // calculate ending sector

        if( sectorStart >= sdCard.SCapacity || sectorEnd >= sdCard.SCapacity ) { // are we out of range?
            sdCard.LastStatus   = SCSI_ST_CHECK_CONDITION;
            sdCard.SCSI_SK      = SCSI_E_IllegalRequest;
            sdCard.SCSI_ASC     = SCSI_ASC_LBA_OUT_OF_RANGE;
            sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;

            PIO_read(sdCard.LastStatus);                            // return error
            return;
        }
    }

    // for sector read commands
    if(justCmd == SCSI_C_READ6 || justCmd == SCSI_C_READ10) {
        longTimeout_basedOnSectorCount(sectorCount);                // set timeout time based on how many sectors are transfered

        res = mmcRead_dma(sectorStart, sectorCount);                // read data
        handled = TRUE;
        wasRead = TRUE;

        timerSetup_cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT);       // after SD access restore short timeout value
    }

    // for sector write commands
    if(justCmd == SCSI_C_WRITE6 || justCmd == SCSI_C_WRITE10) {
        longTimeout_basedOnSectorCount(sectorCount);                // set timeout time based on how many sectors are transfered

        res = mmcWrite_dma(sectorStart, sectorCount);               // write data
        handled  = TRUE;
        wasWrite = TRUE;

        timerSetup_cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT);       // after SD access restore short timeout value
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

    if(res != 0 || pioReadFailed) {                                 // if the result was BAD, or PIO read was BAD
        if(wasRead) {                                               // on failed read, increment this counter
            sdErrorCountRead++;
        }

        if(wasWrite) {                                              // on failed write, increment that counter
            sdErrorCountWrite++;
        }
    }

    // verify command handling
    if(justCmd == SCSI_C_VERIFY) {
        longTimeout_basedOnSectorCount(sectorCount);                // set timeout time based on how many sectors are transfered

        res = mmcCompareMore(sectorStart, sectorCount);             // compare sectors

        timerSetup_cmdTimeoutChangeLength(CMD_TIMEOUT_SHORT);       // after SD access restore short timeout value

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

        case SCSI_C_REQUEST_SENSE:      SCSI_RequestSense(lun);                 return;

        case SCSI_C_FORMAT_UNIT:        SCSI_FormatUnit();                      return;
        
        case SCSI_C_READ_CAPACITY:      SCSI_ReadCapacity();                    return;

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
            sdCard.SCSI_ASCQ    = SCSI_ASCQ_NO_ADDITIONAL_SENSE;

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
    DMA_read(0);            // fixed to 512 B
    DMA_read(0);
    DMA_read(2);
    DMA_read(0);

    scsi_sendOKstatus();
}

// return the last error that occured
void SCSI_RequestSense(BYTE lun)
{
    TDevice *device;
    TDevice badLunDevice;

    char i,xx;
    BYTE val;

    // pre-fill the bad device SC, ASC, ASCQ
    badLunDevice.SCSI_SK    = SCSI_E_IllegalRequest;
    badLunDevice.SCSI_ASC   = SCSI_ASC_LU_NOT_SUPPORTED;
    badLunDevice.SCSI_ASCQ = 0;

    // if LUN is zero, use SD card info, if LUN is non-zero, use bad device info
    device = (lun == 0) ? &sdCard : &badLunDevice;

    // this command clears the unit attention state
    if(sdCard.MediaChanged == TRUE)	{
        scsi_clearTheUnitAttention();
    }

    xx = cmd[4];		  // how many bytes should be sent

    ACSI_DATADIR_READ();

    for(i=0; i<xx; i++)	{
        switch(i) {
            case  0:    val = 0xf0;                 break;  // error code 
            case  2:    val = device->SCSI_SK;      break;  // sense key 
            case  7:    val = xx-7;                 break;  // AS length
            case 12:    val = device->SCSI_ASC;     break;  // additional sense code
            case 13:    val = device->SCSI_ASCQ;    break;  // additional sense code qualifier

            default:    val = 0;                break;
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

    //                      0   0   0   0   0   0   0   0   001111111111222222222233333333334444 
    //                      0   1   2   3   4   5   6   7   890123456789012345678901234567890123 
    char inquiryData[44] = "\x00\x80\x02\x02\x27\x00\x00\x00JOOKIE  CosmosEx 0 SD   2.0001/08/16";
	
    char *inquiryName;
    char inquiryNameWithRpi[10] = {"CosmosEx  "};
    char inquiryNameSolo[10]    = {"CosmoSolo "};

    if(firstConfigReceived) {
        inquiryName = inquiryNameWithRpi;
    } else {
        inquiryName = inquiryNameSolo;
    }

    if(lun == 0) {          // for LUN 0
        firstByte = 0;
    } else {                // for other LUNs
        firstByte = 0x7f;   // peripheralQualifier = 0x03, deviceType = 0x1f
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
    // prepare the inquiry data
    inquiryData[ 0] = firstByte;                            // first byte depends on LUN #
    inquiryData[25] = '0' + sdCardID;                       // sd card id 
    memcpy(inquiryData + 16, inquiryName,           9);     // inquiry name depends on solo or normal mode
    memcpy(inquiryData + 32, VERSION_STRING_SHORT,  4);     // version string is fixed (compiled) in FW
    memcpy(inquiryData + 36, DATE_STRING,           8);     // date    string is fixed (compiled) in FW

	//-----------	
	xx = cmd[4];		  									// how many bytes should be sent
    
	ACSI_DATADIR_READ();

	for(i=0; i<xx; i++)	{
        if(i<=43) {                     // first 44 bytes come from the inquiry data array
            val = inquiryData[i];
		} else {                        // for other locations init on ZERO
    		val = 0;
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

void scsi_log_init(void)
{
    BYTE i,j;
    
    for(i=0; i<SCSI_LOG_LENGTH; i++) {
        
        for(j=0; j<14; j++) {
            scsiLogs[i].cmd[j] = 0;
        }
        scsiLogs[i].res = 0;
    }
    
    scsiLogNow = 0;
}

void scsi_log_add(void)
{
    BYTE j;
    
    for(j=0; j<14; j++) {
        scsiLogs[scsiLogNow].cmd[j] = cmd[j];
    }
    
    scsiLogs[scsiLogNow].res = sdCard.LastStatus;
    
    scsiLogNow++;
    scsiLogNow = scsiLogNow & SCSI_LOG_MASK;
}

void memcpy(char *dest, char *src, int cnt)
{
    int i;

    for(i=0; i<cnt; i++) {
        dest[i] = src[i];
    }
}

