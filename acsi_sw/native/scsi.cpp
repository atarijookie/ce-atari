#include "scsi_defs.h"
#include "scsi.h"

#include "../global.h"

//----------------------------
void CScsi::ProcSCSI6(void)
{
    BYTE justCmd;

    shitHasHappened = 0;

    if((cmd[1] & 0xE0) != 0x00)   			  	// if device ID isn't ZERO
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK	= SCSI_E_IllegalRequest;
        devInfo.SCSI_ASC	= SCSI_ASC_LU_NOT_SUPPORTED;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        PIO_read(devInfo.LastStatus);   // send status byte

        return;
    }

    justCmd = cmd[0] & 0x1f;				// get only the command part of byte

    //----------------
    // now to solve the not initialized device
    if(devInfo.IsInit != true)
    {
        // for the next 3 commands the device is not ready
        if((justCmd == SCSI_C_FORMAT_UNIT) || (justCmd == SCSI_C_READ6) || (justCmd == SCSI_C_WRITE6))
        {
            ReturnStatusAccordingToIsInit();
            return;
        }
    }
    //----------------
    // if media changed, and the command is not INQUIRY and REQUEST SENSE
    if(devInfo.MediaChanged == true)
    {
        if((justCmd != SCSI_C_INQUIRY) && (justCmd != SCSI_C_REQUEST_SENSE))
        {
            ReturnUnitAttention();
            return;
        }
    }
    //----------------
    //	showCommand(0xe0, 6, devInfo.LastStatus);

    switch(justCmd)
    {
    case SCSI_C_SEND_DIAGNOSTIC:
    case SCSI_C_RESERVE:
    case SCSI_C_RELEASE:
    case SCSI_C_TEST_UNIT_READY:    ReturnStatusAccordingToIsInit();    return;

    case SCSI_C_MODE_SENSE6:        SCSI_ModeSense6();                  return;

    case SCSI_C_REQUEST_SENSE:      SCSI_RequestSense();                return;
    case SCSI_C_INQUIRY:            SCSI_Inquiry();                     return;

    case SCSI_C_FORMAT_UNIT:        SCSI_FormatUnit();                  return;
    case SCSI_C_READ6:              SCSI_ReadWrite6(true);              return;
    case SCSI_C_WRITE6:             SCSI_ReadWrite6(false);             return;
        //----------------------------------------------------
    default:
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK		= SCSI_E_IllegalRequest;
        devInfo.SCSI_ASC	= SCSI_ASC_InvalidCommandOperationCode;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        //		showCommand(0xf0, 6, devInfo.LastStatus);

        PIO_read(devInfo.LastStatus);   // send status byte
        break;
    }
    }
}
//----------------------------------------------
void CScsi::ReturnUnitAttention(void)
{
    devInfo.MediaChanged = false;

    devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
    devInfo.SCSI_SK     = SCSI_E_UnitAttention;
    devInfo.SCSI_ASC	= SCSI_ASC_NOT_READY_TO_READY_TRANSITION;
    devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
    if(shitHasHappened)
        showCommand(1, 6, devInfo.LastStatus);

    shitHasHappened = 0;
#endif

    PIO_read(devInfo.LastStatus);   // send status byte
}
//----------------------------------------------
void CScsi::ReturnStatusAccordingToIsInit(void)
{
    if(devInfo.IsInit == true)
        SendOKstatus();
    else
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK     = SCSI_E_NotReady;
        devInfo.SCSI_ASC	= SCSI_ASC_MEDIUM_NOT_PRESENT;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
        if(shitHasHappened)
            showCommand(2, 6, devInfo.LastStatus);

        shitHasHappened = 0;
#endif

        PIO_read(devInfo.LastStatus);   // send status byte
    }
}
//----------------------------------------------
void CScsi::SendOKstatus(void)
{
    devInfo.LastStatus	= SCSI_ST_OK;
    devInfo.SCSI_SK	= SCSI_E_NoSense;
    devInfo.SCSI_ASC	= SCSI_ASC_NO_ADDITIONAL_SENSE;
    devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
    if(shitHasHappened)
        showCommand(3, 6, devInfo.LastStatus);

    shitHasHappened = 0;
#endif

    PIO_read(devInfo.LastStatus);   // send status byte, long time-out
}
//----------------------------------------------
BYTE CScsi::SCSI_Read6_SDMMC(DWORD sector, WORD lenX)
{
    BYTE res = 0;

#define READ_SINGLE
    //--------------------
#ifndef READ_SINGLE

    if(lenX==1)
    {
        res = mmcRead(, sector);
    }
    else
    {
        res = mmcReadMore(, sector, lenX);
    }
#endif
    //--------------------
#ifdef READ_SINGLE
    WORD i;

    for(i=0; i<lenX; i++)					// all needed sectors
    {
        if(sector >= devInfo.SCapacity)
            return 1;

        res = mmcRead(sector);		// write

        if(res!=0)							// if error, then break
            break;

        sector++;							// next sector
    }
#endif
    /*
    if(sector < 0xff)
        wait_ms(10);
*/	
    return res;
}
//----------------------------------------------
BYTE CScsi::SCSI_Write6_SDMMC(DWORD sector, WORD lenX)
{
    BYTE res = 0;

    if(lenX == 1)									// just 1 sector?
    {
        if(sector >= devInfo.SCapacity)
            return 1;

        res = mmcWrite(sector);
    }
    else
    {
        res = mmcWriteMore(sector, lenX);
    }

    return res;
}
//----------------------------------------------
void CScsi::SCSI_ReadWrite6(BYTE Read)
{
    DWORD sector;
    BYTE res = 0;
    WORD lenX;

    sector  = (cmd[1] & 0x1f);
    sector  = sector << 8;
    sector |= cmd[2];
    sector  = sector << 8;
    sector |= cmd[3];

    lenX = cmd[4];	   	 	   	  // get the # of sectors to read

    if(lenX==0)
        lenX=256;
    //--------------------------------
    if(Read==true)				// if read
        res = SCSI_Read6_SDMMC(sector, lenX);
    else						// if write
        res = SCSI_Write6_SDMMC(sector, lenX);
    //--------------------------------
    if(res==0)			   							// if everything was OK
    {
        SendOKstatus();
    }
    else 							   			   // if error
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK		= SCSI_E_MediumError;
        devInfo.SCSI_ASC		= SCSI_ASC_NO_ADDITIONAL_SENSE;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
        if(shitHasHappened)
            showCommand(4, 6, devInfo.LastStatus);

        shitHasHappened = 0;
#endif

        PIO_read(devInfo.LastStatus);    // send status byte, long time-out
    }
}
//----------------------------------------------
void CScsi::SCSI_FormatUnit(void)
{
    BYTE res = 0;

    res = EraseCard();
    //---------------
    if(res==0)			   							// if everything was OK
    {
        wait_ms(1000);

        SendOKstatus();
    }
    else 							   			   // if error
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK	= SCSI_E_MediumError;
        devInfo.SCSI_ASC	= SCSI_ASC_NO_ADDITIONAL_SENSE;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
        if(shitHasHappened)
            showCommand(5, 6, devInfo.LastStatus);

        shitHasHappened = 0;
#endif

        PIO_read(devInfo.LastStatus);    // send status byte, long time-out
    }
}
//----------------------------------------------
void CScsi::ClearTheUnitAttention(void)
{
    devInfo.LastStatus	= SCSI_ST_OK;
    devInfo.SCSI_SK		= SCSI_E_NoSense;
    devInfo.SCSI_ASC		= SCSI_ASC_NO_ADDITIONAL_SENSE;
    devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

    devInfo.MediaChanged = false;
}
//----------------------------------------------
void CScsi::SCSI_Inquiry(void)
{
    WORD i,xx;
    BYTE val;

    BYTE vendor[8] = {"JOOKIE  "};

    if(devInfo.MediaChanged == true)                       // this command clears the unit attention state
        ClearTheUnitAttention();

    if(cmd[1] & 0x01)                                               // EVPD bit is set? Request for vital data?
    {                                                               // vital data not suported
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK	= SCSI_E_IllegalRequest;
        devInfo.SCSI_ASC	= SCSO_ASC_INVALID_FIELD_IN_CDB;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        PIO_read(devInfo.LastStatus);    // send status byte, long time-out

        return;
    }

    //-----------
    xx = cmd[4];		  									// how many bytes should be sent

    for(i=0; i<xx; i++)
    {

        if(i >= 8 && i<=43) {           // if the returned byte is somewhere from ASCII part of data, init on 'space' character
            val = ' ';
        } else {                        // for other locations init on ZERO
            val = 0;
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

        if(i>=16 && i<=25) {            // send device name (UltraSatan)
            val = InquiryName[i-16];
        }

        if(i == 27) {                   // send slot # (1 or 2)
            val = '1' + ;
        }

        if(i>=32 && i<=35) {            // version string
            val = VERSION_STRING_SHORT[i-32];
        }

        if(i>=36 && i<=43) {            // date string
            val = DATE_STRING[i-36];
        }

        DMA_read(val);

        if(brStat != E_OK)  // if something isn't OK
        {
#ifdef DEBUG_OUT
            fputs(str_e);
#endif

            return;		  								// quit this
        }
    }

    SendOKstatus();
}
//----------------------------------------------
void CScsi::SCSI_ModeSense6(void)
{
    WORD length, i, len;
    BYTE PageCode, val;
    //-----------------
    BYTE page_control[]	= {0x0a, 0x06, 0, 0, 0, 0, 0, 0};
    BYTE page_medium[]	= {0x0b, 0x06, 0, 0, 0, 0, 0, 0};
    //-----------------
    PageCode	= cmd[2] & 0x3f;	// get only page code
    length		= cmd[4];		  		// how many bytes should be sent

    //-----------------
    // page not supported?
    if(PageCode != 0x0a && PageCode != 0x0b && PageCode != 0x3f)
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK	= SCSI_E_IllegalRequest;
        devInfo.SCSI_ASC	= SCSO_ASC_INVALID_FIELD_IN_CDB;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        PIO_read(devInfo.LastStatus);    // send status byte, long time-out

        return;
    }
    //-----------------
    // send the page

    switch(PageCode)
    {
    case 0x0a:
    case 0x0b:	len = 8;			break;
    case 0x3f:	len = 8 + 8;	break;
    default:		len = 0;			break;
    }

    for(i=0; i<length; i++)
    {
        val = 0;									// send 0 by default?

        if(i==0)									// Mode parameter header - Mode data length?
            val = 3 + len;

        if(PageCode == 0x0a)			// should send control page?
        {
            if(i>=4 && i<=11)
                val = page_control[i - 4];
        }

        if(PageCode == 0x0b)			// should send medium page?
        {
            if(i>=4 && i<=11)
                val = page_medium[i - 4];
        }

        if(PageCode == 0x3f)			// should send all pages?
        {
            if(i>=4 && i<=11)
                val = page_control[i - 4];

            if(i>=12 && i<=19)
                val = page_medium[i - 12];
        }

        DMA_read(val);

        if(brStat != E_OK)		  			   		 		// if something isn't OK
        {
#ifdef DEBUG_OUT
            fputs(str_e);
#endif

            return;		  								// quit this
        }
    }

    SendOKstatus();
}
//----------------------------------------------
// return the last error that occured
void CScsi::SCSI_RequestSense(void)
{
    char i,xx; //, res;
    unsigned char val;

    if(devInfo.MediaChanged == true)	// this command clears the unit attention state
        ClearTheUnitAttention();

    xx = cmd[4];		  // how many bytes should be sent

    for(i=0; i<xx; i++)
    {
        switch(i)
        {
        case  0:	val = 0xf0;							break;		// error code
        case  2:	val = devInfo.SCSI_SK;		break;		// sense key
        case  7:	val = xx-7;							break;		// AS length
        case 12:	val = devInfo.SCSI_ASC;	break;		// additional sense code
        case 13:	val = devInfo.SCSI_ASCQ;	break;		// additional sense code qualifier

        default:	val = 0; 			   				break;
        }

        DMA_read(val);

        if(brStat != E_OK)		  			   		 		// if something isn't OK
        {
#ifdef DEBUG_OUT
            fputs(str_e);
#endif

            return;		  								// quit this
        }
    }

    SendOKstatus();
}
//----------------------------------------------
void CScsi::SendEmptySecotrs(WORD sectors)
{
    WORD i,j;
    BYTE r1;

    for(j=0; j<sectors; j++)
    {
        for(i=0; i<512; i++)
        {
            r1 = DMA_read(0);

            if(brStat != E_OK)		  						// if something was wrong
            {
                uart_outhexD(i);
                uart_putchar('\n');

                return;
            }
        }
    }
}
//----------------------------------------------
void CScsi::showCommand(WORD id, WORD length, WORD errCode)
{
    char tmp[64];

    memset(tmp, 0, 64);
    sprintf(tmp, "%d - ", id);

    WORD i;

    for(i=0; i<length; i++) {
        sprintf(tmp + 4 + i*3, "%02x ", cmd[i]);
    }

    int len = strlen(tmp);
    sprintf(tmp + len, "- %02x", errCode);
}
//----------------------------------------------
void CScsi::ProcICD(void)
{
    shitHasHappened = 0;

#define WRITEOUT

    //----------------
    // 1st we need to process the UltraSatan's special commands
    // Their format is as follows:
    // cmd[0] - (ACSI ID << 5) | 0x1f   - command in ICD format
    // cmd[1] - 0x20     - group code 1 (1 + 10 bytes long command) and command TEST UNIT READY
    // cmd[2..3]         - the 'US' string (US as UltraSatan)
    // cmd[4..7]         - special command code / string
    // cmd[8..10]        - 3 bytes of parameters

    // so the complete command could look like this:
    // 0x1f, 0x20, 'USRdFW', 0x01, 0x0010  (read sector 0x0010 of firmware 1)

    if(cmd[2]=='U' && cmd[3]=='S')									// some UltraSatan's specific commands?
    {
        if(!cmpn(&cmd[4], "RdFW", 4))									// firmware read?
        {
            Special_ReadFW();
            return;
        }
        //---------
        if(!cmpn(&cmd[4], "WrFW", 4))									// firmware write?
        {
            Special_WriteFW();
            return;
        }
        //---------
        if(!cmpn(&cmd[4], "RdSt", 4))									// read settings?
        {
            Special_ReadSettings();
            return;
        }
        //---------
        if(!cmpn(&cmd[4], "WrSt", 4))									// write settings?
        {
            Special_WriteSettings();
            return;
        }
        //---------
        if(!cmpn(&cmd[4], "RdCl", 4))									// read clock?
        {
            Special_ReadRTC();
            return;
        }
        //---------
        if(!cmpn(&cmd[4], "WrCl", 4))									// write clock?
        {
            Special_WriteRTC();
            return;
        }
        //---------
        if(!cmpn(&cmd[4], "CurntFW", 7))							// read the name of currently running FW?
        {
            Special_ReadCurrentFirmwareName();
            return;
        }
        //---------
        if(!cmpn(&cmd[4], "RdINQRN", 7))									// read INQUIRY name?
        {
            Special_ReadInquiryName();
            return;
        }
        //---------
        if(!cmpn(&cmd[4], "WrINQRN", 7))									// write INQUIRY name?
        {
            Special_WriteInquiryName();
            return;
        }
        //---------
    }
    //----------------
    if((cmd[2] & 0xE0) != 0x00)   			  					// if device ID isn't ZERO
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK	= SCSI_E_IllegalRequest;		// other devices = error
        devInfo.SCSI_ASC	= SCSI_ASC_LU_NOT_SUPPORTED;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        PIO_read(devInfo.LastStatus);   // send status byte

        return;
    }
    //----------------
    // now for the not present media
    if(devInfo.IsInit != true)
    {
        // for the next 3 commands the device is not ready
        if((cmd[1] == SCSI_C_READ10) || (cmd[1] == SCSI_C_WRITE10) || (cmd[1] == SCSI_C_READ_CAPACITY))
        {
            ReturnStatusAccordingToIsInit();
            return;
        }
    }
    //----------------
    // if media changed, and the command is not INQUIRY and REQUEST SENSE
    if(devInfo.MediaChanged == true)
    {
        if(cmd[1] != SCSI_C_INQUIRY)
        {
            ReturnUnitAttention();
            return;
        }
    }
    //----------------
    //	showCommand(0xe1, 12, 0);


    switch(cmd[1])
    {
    case SCSI_C_READ_CAPACITY:
        SCSI_ReadCapacity();
        break;

    case SCSI_C_INQUIRY:
        ICD7_to_SCSI6();
        SCSI_Inquiry();
        break;
        //------------------------------
    case SCSI_C_READ10:				SCSI_ReadWrite10(, true); break;
    case SCSI_C_WRITE10:			SCSI_ReadWrite10(, false); break;
        //----------------------------------------------------
    case SCSI_C_VERIFY:				SCSI_Verify(); break;

        //----------------------------------------------------
    default:
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK		= SCSI_E_IllegalRequest;		// other devices = error
        devInfo.SCSI_ASC		= SCSI_ASC_InvalidCommandOperationCode;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        //		showCommand(0xf1, 12, devInfo.LastStatus);

        PIO_read(devInfo.LastStatus);   // send status byte
        break;
    }
} 
//---------------------------------------------
void CScsi::SCSI_Verify(void)
{
    DWORD sector;
    BYTE res=0;
    WORD lenX, i;
    BYTE foundError;

    sector  = cmd[3];			// get starting sector #
    sector  = sector << 8;
    sector |= cmd[4];
    sector  = sector << 8;
    sector |= cmd[5];
    sector  = sector << 8;
    sector |= cmd[6];

    lenX  = cmd[8];	  	   		// get the # of sectors to read
    lenX  = lenX << 8;
    lenX |= cmd[9];

    foundError = 0;					// no error found yet

    if((cmd[2] & 0x02) == 0x02)		// BytChk == 1? : compare with data
    {
        for(i=0; i<lenX; i++)					// all needed sectors
        {
            if(sector >= devInfo.SCapacity)	// out of bounds?
                break;									// stop right now

            res = mmcCompare(, sector);	// compare data

            if(res!=0)							// if error, then set flag
                foundError = 1;

            sector++;							// next sector
        }

        if(foundError)							// problem when comparing?
        {
            devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
            devInfo.SCSI_SK	= SCSI_E_Miscompare;
            devInfo.SCSI_ASC	= SCSI_ASC_VERIFY_MISCOMPARE;
            devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

            PIO_read(devInfo.LastStatus);   // send status byte
        }
        else									// no problem?
        {
            SendOKstatus();
        }
    }
    else							// BytChk == 0? : no data comparison
    {								// just send all OK
        SendOKstatus();
    }
}
//---------------------------------------------
void CScsi::SCSI_ReadCapacity(void)
{	 // return disk capacity and sector size
    DWORD cap;
    BYTE hi,midlo, midhi, lo;

    cap = devInfo.SCapacity;
    cap--;

    hi		= (cap >> 24) & 0xff;
    midhi	= (cap >> 16) & 0xff;
    midlo	= (cap >>  8) & 0xff;
    lo		=  cap        & 0xff;

    if(devInfo.IsInit != true)
    {
        hi		= 0;
        midhi	= 0;
        midlo	= 0;
        lo		= 0;
    }

    DMA_read(hi);		 	// Hi
    DMA_read(midhi);	// mid-Hi
    DMA_read(midlo);	// mid-Lo
    DMA_read(lo);		 	// Lo

    // return sector size
    DMA_read(0);				 // fixed to 512 B
    DMA_read(0);
    DMA_read(2);
    DMA_read(0);

    PostDMA_read();

    SendOKstatus();
}
//---------------------------------------------
void CScsi::ICD7_to_SCSI6(void)
{
    cmd[0] = cmd[1];
    cmd[1] = cmd[2];
    cmd[2] = cmd[3];
    cmd[3] = cmd[4];
    cmd[4] = cmd[5];
    cmd[5] = cmd[6];
}
//---------------------------------------------
void CScsi::SCSI_ReadWrite10(char Read)
{
    DWORD sector;
    BYTE res=0;
    WORD lenX;

    sector  = cmd[3];
    sector  = sector << 8;
    sector |= cmd[4];
    sector  = sector << 8;
    sector |= cmd[5];
    sector  = sector << 8;
    sector |= cmd[6];

    lenX  = cmd[8];	  	   		// get the # of sectors to read
    lenX  = lenX << 8;
    lenX |= cmd[9];
    //--------------------------------
    if(Read==true)				// if read
        res = SCSI_Read6_SDMMC(sector, lenX);
    else									// if write
        res = SCSI_Write6_SDMMC(sector, lenX);
    //--------------------------------
    if(res==0)							// if everything was OK
    {
        SendOKstatus();
    }
    else									// if error
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK		= SCSI_E_MediumError;
        devInfo.SCSI_ASC		= SCSI_ASC_NO_ADDITIONAL_SENSE;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
        if(shitHasHappened)
            showCommand(0xc1, 12, devInfo.LastStatus);

        shitHasHappened = 0;
#endif

        PIO_read(devInfo.LastStatus);    // send status byte, long time-out
    }
}
//----------------------------------------------
