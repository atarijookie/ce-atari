#include <string.h>
#include <stdio.h>

#include "scsi_defs.h"
#include "scsi.h"
#include "../global.h"

// TODO:
// read operacie budu dobre, problem bude ak pri write nastane error, pretoze momentalne je status posielany cez SPI najprv

extern "C" void outDebugString(const char *format, ...);

#define BUFFER_SIZE             (1024*1024)
#define BUFFER_SIZE_SECTORS     (BUFFER_SIZE / 512)

Scsi::Scsi(void)
{
    dataTrans = 0;
    dataMedia = 0;
    strncpy((char *) inquiryName, "CosmosEx  ", 10);

    dataBuffer  = new BYTE[BUFFER_SIZE];
    dataBuffer2 = new BYTE[BUFFER_SIZE];
}

Scsi::~Scsi()
{
    delete []dataBuffer;
    delete []dataBuffer2;
}

void Scsi::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void Scsi::setDataMedia(DataMedia *dm)
{
    dataMedia = dm;
}

void Scsi::processCommand(BYTE *command)
{
    if(dataTrans == 0 || dataMedia == 0) {
        outDebugString("processCommand was called without valid dataTrans or dataMedia!");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    cmd = command;

    if(isICDcommand()) {	  			// if it's a ICD command
        ProcICD();
    } else {      						// if it's a normal command
        ProcScsi6();
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}

bool Scsi::isICDcommand(void)
{
    if((cmd[0] & 0x1f)==0x1f) {			  // if the command is '0x1f'
        return true;
    }

    return false;
}

void Scsi::ProcScsi6(void)
{
    BYTE justCmd;

    shitHasHappened = 0;

    if((cmd[1] & 0xE0) != 0x00)   			  	// if device ID isn't ZERO
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK     = SCSI_E_IllegalRequest;
        devInfo.SCSI_ASC	= SCSI_ASC_LU_NOT_SUPPORTED;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        dataTrans->setStatus(devInfo.LastStatus);   // send status byte

        return;
    }

    justCmd = cmd[0] & 0x1f;				// get only the command part of byte

    //----------------
    // now to solve the not initialized device
    if(!dataMedia->isInit())
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
    if(dataMedia->mediaChanged())
    {
        if((justCmd != SCSI_C_INQUIRY) && (justCmd != SCSI_C_REQUEST_SENSE))
        {
            ReturnUnitAttention();
            return;
        }
    }
    //----------------
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

        dataTrans->setStatus(devInfo.LastStatus);   // send status byte
        break;
        }
    }
}
//----------------------------------------------
void Scsi::ReturnUnitAttention(void)
{
    dataMedia->setMediaChanged(false);

    devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
    devInfo.SCSI_SK     = SCSI_E_UnitAttention;
    devInfo.SCSI_ASC	= SCSI_ASC_NOT_READY_TO_READY_TRANSITION;
    devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

    dataTrans->setStatus(devInfo.LastStatus);   // send status byte
}
//----------------------------------------------
void Scsi::ReturnStatusAccordingToIsInit(void)
{
    if(dataMedia->isInit())
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

        dataTrans->setStatus(devInfo.LastStatus);   // send status byte
    }
}
//----------------------------------------------
void Scsi::SendOKstatus(void)
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

    dataTrans->setStatus(devInfo.LastStatus);   // send status byte, long time-out
}
//----------------------------------------------
void Scsi::SCSI_ReadWrite6(bool read)
{
    DWORD sector;
    bool res;
    WORD lenX;

    sector  = (cmd[1] & 0x1f);
    sector  = sector << 8;
    sector |= cmd[2];
    sector  = sector << 8;
    sector |= cmd[3];

    lenX = cmd[4];	   	 	   	  // get the # of sectors to read

    if(lenX == 0) {
        lenX = 256;
    }
    //--------------------------------
    if(read) {      			// if read
        res = readSectors(sector, lenX);
    } else {					// if write
        res = writeSectors(sector, lenX);
    }
    //--------------------------------
    if(res) { 			   							// if everything was OK
        SendOKstatus();
    } else { 							   			// if error
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK		= SCSI_E_MediumError;
        devInfo.SCSI_ASC	= SCSI_ASC_NO_ADDITIONAL_SENSE;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        dataTrans->setStatus(devInfo.LastStatus);    // send status byte, long time-out
    }
}
//----------------------------------------------
void Scsi::SCSI_FormatUnit(void)
{
    BYTE res = 0;

    res = eraseMedia();
    //---------------
    if(res) { 			   							// if everything was OK
        SendOKstatus();
    } else {						   			   // if error
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK	= SCSI_E_MediumError;
        devInfo.SCSI_ASC	= SCSI_ASC_NO_ADDITIONAL_SENSE;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        dataTrans->setStatus(devInfo.LastStatus);    // send status byte, long time-out
    }
}
//----------------------------------------------
void Scsi::ClearTheUnitAttention(void)
{
    devInfo.LastStatus	= SCSI_ST_OK;
    devInfo.SCSI_SK		= SCSI_E_NoSense;
    devInfo.SCSI_ASC	= SCSI_ASC_NO_ADDITIONAL_SENSE;
    devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

    dataMedia->setMediaChanged(false);
}
//----------------------------------------------
void Scsi::SCSI_Inquiry(void)
{
    WORD i,xx;
    BYTE val;

    BYTE *vendor = (BYTE *) "JOOKIE  ";

    if(dataMedia->mediaChanged())                                   // this command clears the unit attention state
        ClearTheUnitAttention();

    if(cmd[1] & 0x01)                                               // EVPD bit is set? Request for vital data?
    {                                                               // vital data not suported
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK	= SCSI_E_IllegalRequest;
        devInfo.SCSI_ASC	= SCSO_ASC_INVALID_FIELD_IN_CDB;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        dataTrans->setStatus(devInfo.LastStatus);    // send status byte, long time-out

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

        if(i>=16 && i<=25) {            // send device name (CosmosEx)
            val = inquiryName[i-16];
        }

        if(i == 27) {                   // send ACSI ID # (0 .. 7)
            val = '0' + devInfo.ACSI_ID;
        }

        if(i>=32 && i<=35) {            // version string
            val = VERSION_STRING_SHORT[i-32];
        }

        if(i>=36 && i<=43) {            // date string
            val = DATE_STRING[i-36];
        }

        dataTrans->addData(val);
    }

    SendOKstatus();
}
//----------------------------------------------
void Scsi::SCSI_ModeSense6(void)
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

        dataTrans->setStatus(devInfo.LastStatus);    // send status byte, long time-out

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

        dataTrans->addData(val);
    }

    SendOKstatus();
}
//----------------------------------------------
// return the last error that occured
void Scsi::SCSI_RequestSense(void)
{
    char i,xx;
    unsigned char val;

    if(dataMedia->mediaChanged())   	// this command clears the unit attention state
        ClearTheUnitAttention();

    xx = cmd[4];                        // how many bytes should be sent

    for(i=0; i<xx; i++)
    {
        switch(i)
        {
        case  0:	val = 0xf0;					break;		// error code
        case  2:	val = devInfo.SCSI_SK;		break;		// sense key
        case  7:	val = xx-7;					break;		// AS length
        case 12:	val = devInfo.SCSI_ASC;     break;		// additional sense code
        case 13:	val = devInfo.SCSI_ASCQ;	break;		// additional sense code qualifier

        default:	val = 0; 			   		break;
        }

        dataTrans->addData(val);
    }

    SendOKstatus();
}
//----------------------------------------------
void Scsi::SendEmptySecotrs(WORD sectors)
{
    WORD i,j;

    for(j=0; j<sectors; j++)
    {
        for(i=0; i<512; i++)
        {
            dataTrans->addData(0);
        }
    }
}
//----------------------------------------------
void Scsi::showCommand(WORD id, WORD length, WORD errCode)
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
void Scsi::ProcICD(void)
{
    shitHasHappened = 0;

#define WRITEOUT

    //----------------
    if((cmd[2] & 0xE0) != 0x00)   			  					// if device ID isn't ZERO
    {
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK     = SCSI_E_IllegalRequest;		// other devices = error
        devInfo.SCSI_ASC	= SCSI_ASC_LU_NOT_SUPPORTED;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        dataTrans->setStatus(devInfo.LastStatus);   // send status byte

        return;
    }
    //----------------
    // now for the not present media
    if(!dataMedia->isInit())
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
    if(dataMedia->mediaChanged())
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
    case SCSI_C_READ10:				SCSI_ReadWrite10(true); break;
    case SCSI_C_WRITE10:			SCSI_ReadWrite10(false); break;
        //----------------------------------------------------
    case SCSI_C_VERIFY:				SCSI_Verify(); break;

        //----------------------------------------------------
    default:
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK		= SCSI_E_IllegalRequest;		// other devices = error
        devInfo.SCSI_ASC		= SCSI_ASC_InvalidCommandOperationCode;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        //		showCommand(0xf1, 12, devInfo.LastStatus);

        dataTrans->setStatus(devInfo.LastStatus);   // send status byte
        break;
    }
} 
//---------------------------------------------
void Scsi::SCSI_Verify(void)
{
    DWORD sector;
    BYTE res=0;
    WORD lenX;

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

    if((cmd[2] & 0x02) == 0x02) {   		// BytChk == 1? : compare with data
        res = compareSectors(sector, lenX);	// compare data

        if(!res) {							// problem when comparing?
            devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
            devInfo.SCSI_SK     = SCSI_E_Miscompare;
            devInfo.SCSI_ASC	= SCSI_ASC_VERIFY_MISCOMPARE;
            devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

            dataTrans->setStatus(devInfo.LastStatus);   // send status byte
        } else {									// no problem?
            SendOKstatus();
        }
    } else {						// BytChk == 0? : no data comparison
        SendOKstatus();
    }
}
//---------------------------------------------
void Scsi::SCSI_ReadCapacity(void)
{	 // return disk capacity and sector size
    DWORD cap;
    BYTE hi,midlo, midhi, lo;

    DWORD scap, bcap;
    dataMedia->getCapacity(bcap, scap);

    cap = scap - 1;

    hi		= (cap >> 24) & 0xff;
    midhi	= (cap >> 16) & 0xff;
    midlo	= (cap >>  8) & 0xff;
    lo		=  cap        & 0xff;

    if(!dataMedia->isInit())
    {
        hi		= 0;
        midhi	= 0;
        midlo	= 0;
        lo		= 0;
    }

    dataTrans->addData(hi);		 	// Hi
    dataTrans->addData(midhi);	// mid-Hi
    dataTrans->addData(midlo);	// mid-Lo
    dataTrans->addData(lo);		 	// Lo

    // return sector size
    dataTrans->addData(0);				 // fixed to 512 B
    dataTrans->addData(0);
    dataTrans->addData(2);
    dataTrans->addData(0);

    SendOKstatus();
}
//---------------------------------------------
void Scsi::ICD7_to_SCSI6(void)
{
    cmd[0] = cmd[1];
    cmd[1] = cmd[2];
    cmd[2] = cmd[3];
    cmd[3] = cmd[4];
    cmd[4] = cmd[5];
    cmd[5] = cmd[6];
}
//---------------------------------------------
void Scsi::SCSI_ReadWrite10(bool read)
{
    DWORD sector;
    bool res;
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
    if(read) {                  // if read
        res = readSectors(sector, lenX);
    } else {					// if write
        res = writeSectors(sector, lenX);
    }
    //--------------------------------
    if(res) {    							// if everything was OK
        SendOKstatus();
    } else {								// if error
        devInfo.LastStatus	= SCSI_ST_CHECK_CONDITION;
        devInfo.SCSI_SK		= SCSI_E_MediumError;
        devInfo.SCSI_ASC	= SCSI_ASC_NO_ADDITIONAL_SENSE;
        devInfo.SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

        dataTrans->setStatus(devInfo.LastStatus);    // send status byte, long time-out
    }
}
//----------------------------------------------
bool Scsi::readSectors(DWORD sectorNo, DWORD count)
{
    bool res;

    if(count > BUFFER_SIZE_SECTORS) {      // more than BUFFER_SIZE_SECTORS of data at once?
        outDebugString("Scsi::readSectors -- tried to read more than BUFFER_SIZE_SECTORS at once, fail!");
        return false;
    }

    res = dataMedia->readSectors(sectorNo, count, dataBuffer);

    if(!res) {
        return false;
    }

    dataTrans->addData(dataBuffer, count);

    return true;
}

bool Scsi::writeSectors(DWORD sectorNo, DWORD count)
{
    bool res;

    if(count > BUFFER_SIZE_SECTORS) {      // more than BUFFER_SIZE_SECTORS of data at once?
        outDebugString("Scsi::writeSectors -- tried to write more than BUFFER_SIZE_SECTORS at once, fail!");
        return false;
    }

    res = dataTrans->recvData(dataBuffer, count);               // get data from Hans

    if(!res) {
        return false;
    }

    res = dataMedia->writeSectors(sectorNo, count, dataBuffer); // write to media

    if(!res) {
        return false;
    }

    return true;
}

bool Scsi::compareSectors(DWORD sectorNo, DWORD count)
{
    bool res;

    if(count > BUFFER_SIZE_SECTORS) {      // more than BUFFER_SIZE_SECTORS of data at once?
        outDebugString("Scsi::compareSectors -- tried to compare more than BUFFER_SIZE_SECTORS at once, fail!");
        return false;
    }

    res = dataTrans->recvData(dataBuffer, count);               // get data from Hans

    if(!res) {
        return false;
    }

    res = dataMedia->readSectors(sectorNo, count, dataBuffer2); // and get data from media

    if(!res) {
        return false;
    }

    DWORD byteCount = count * 512;
    int iRes = memcmp(dataBuffer, dataBuffer2, byteCount);      // now compare the data

    if(iRes != 0) {                                             // data is different?
        return false;
    }

    return true;
}

bool Scsi::eraseMedia(void)
{
    bool res;

    memset(dataBuffer, 0, 512);                                 // create empty buffer

    for(int i=0; i<100; i++) {                                  // write empty sector to position 0 .. 99
        res = dataMedia->writeSectors(i, 1, dataBuffer);

        if(!res) {                                              // failed to write?
            return false;
        }
    }

    return true;                                                // all good
}
//----------------------------------------------
