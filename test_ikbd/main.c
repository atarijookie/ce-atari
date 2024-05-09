//--------------------------------------------------
#include <mint/osbind.h> 
#include <mint/linea.h> 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../libacsiscsi/acsi.h"
#include "../libacsiscsi/hdd_if.h"
// #include "Cookiejar.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "VT52.h"
#include "version.h"
#include "tests/test.h"
#include "tests/ikbd_txready.h"
#include "tests/ikbd_send.h"
#include "tests/ikbd_reset.h"
#include "tests/mouse_absolute_zero.h"
#include "helper/ikbd.h"

//--------------------------------------------------

void showConnectionErrorMessage(void);
uint8_t findDevice(void);
int getConfig(void); 
int readHansTest (uint32_t byteCount, uint16_t xorVal );
int writeHansTest(uint32_t byteCount, uint16_t xorVal );
void sleep(int seconds);

void print_head(void);
void print_status(void);

void showHexByte(uint8_t val);
void showHexuint32_t(uint32_t val);
void logMsg(char *logMsg);
void deleteErrorLines(void);
void speedTest(void);

uint8_t ce_identify(uint8_t ACSI_id);
//--------------------------------------------------
uint8_t deviceID;

uint8_t commandLong [CMD_LENGTH_LONG ] = {0x1f, 0xA0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0}; 

uint8_t readBuffer [256 * 512];
uint8_t writeBuffer[256 * 512];
uint8_t *rBuffer, *wBuffer;

uint8_t prevCommandFailed;

#define HOSTMOD_CONFIG				1
#define HOSTMOD_LINUX_TERMINAL		2
#define HOSTMOD_TRANSLATED_DISK		3
#define HOSTMOD_NETWORK_ADAPTER		4
#define HOSTMOD_FDD_SETUP           5

#define TRAN_CMD_IDENTIFY           0
#define TRAN_CMD_GETDATETIME        1

#define DATE_OK                              0
#define DATE_ERROR                           2
#define DATE_DATETIME_UNKNOWN                4

#define Clear_home()    (void) Cconws("\33E")

#define ERROR_LINE_START        10

#define SKIP_CEDETECTION

uint8_t ifUsed;

// Warning! Don't use VT52_Save_pos() and VT52_Load_pos(), because they don't work on Falcon! (They work fine on ST and TT.)

int errorLine = 0;
uint8_t simpleNotDetailedErrReport = 1;

typedef struct {
    uint32_t goodPlain;
    uint32_t goodWithRetry;
    uint32_t errorTimeout;
    uint32_t errorCrc;
    uint32_t errorOther;
} Tresults;

struct {
    uint32_t run;
    uint32_t singleOps;
    
    Tresults read;
    Tresults write;
} counts;

void updateCounts(uint8_t doingWriteNotRead, int res);
void printOpResult(int res);

//============================================================================
//test queue
//----------------------------------------------------------------------------
extern TTestIf test_mouse_absolute_zero;
extern TTestIf test_mouse_loadpos;

TTestIf* tests[]=
{
    &test_ikbd_txready,
    &test_ikbd_send,
    &test_ikbd_reset,
    &test_mouse_absolute_zero,
    &test_mouse_loadpos,
    NULL
};
//============================================================================

int testLine;

void runTests(void)
{
    int testid=0;
    uint8_t success=FALSE;
    
    ikbd_disable_irq();                     // disable ikbd irqs, because we're reading the data through polling
    
    testLine = 5;
    
    while( tests[testid]!=NULL )
    {
        success=FALSE;
        VT52_Save_pos();
        
        if(tests[testid]->init) {           // if there is an init function, call it
            (tests[testid]->init)();
        }
        
        success=(tests[testid]->run)();
        
        if(tests[testid]->tearDown) {       // if there is the teardown function, call it
            (tests[testid]->tearDown)();
        }
        
        VT52_Load_pos();
        if( success==FALSE )
        {
            VT52_Rev_on();
            (void) Cconws("FAIL");
            VT52_Rev_off();
        }else{
            (void) Cconws(" OK ");
        }
        (void) Cconws("\r\n");
        testid++;
        
        testLine++;
    }
    
    ikbd_enable_irq();                      // enable ikbd irqs again
    ikbd_puts((const uint8_t []) {0x08}, 1);   // set relative mouse reporting
}

//--------------------------------------------------
int main(void)
{
	uint32_t toEven;
	void *OldSP;

    //----------------------
    // read all the keys which are waiting, so we can ignore them
    while(1) {
        if(Cconis() != 0) {             // if some key is waiting, read it
            Cnecin();
        } else {                        // no key is waiting, quit the loop
            break;
        }
    }
    //----------------------
    
	OldSP = (void *) Super((void *)0);  			// supervisor mode 
//	lineaa();	// hide mouse    
	
	// ---------------------- 
	// create buffer pointer to even address 
	toEven = (uint32_t) &readBuffer[0];
  
	if(toEven & 0x0001)       // not even number? 
		toEven++;
  
	rBuffer = (uint8_t *) toEven; 

    //----------
  	toEven = (uint32_t) &writeBuffer[0];
  
	if(toEven & 0x0001)       // not even number? 
		toEven++;
  
	wBuffer = (uint8_t *) toEven; 

	Clear_home();

	// ---------------------- 
	print_head();
	(void) Cconws("\r\n");
	(void) Cconws(" Interactive test to check consistency\r\n");
    (void) Cconws(" of the CE IKBD injection chain.\r\n");         
    (void) Cconws("\r\n");

    #ifndef SKIP_CEDETECTION 

    uint8_t res = findDevice(FIND_DEV_CE);

    if(res == DEVICE_NOT_FOUND) {
      	linea9();   
	    Super((void *)OldSP);  			      // user mode 
        sleep(3);
        return 0;
    }

    deviceID = res & 0x07;                                  // store the BUS ID of device

    #endif

	// ----------------- 
    // now set up the acsi command bytes so we don't have to deal with this one anymore 
	commandLong [0] = (deviceID << 5) | 0x1f;			// cmd[0] = ACSI_id + ICD command marker (0x1f)	

    // ---------------------- 
    runTests();

    (void) Cconws("Done. Press any key to terminate.\r\n");
    Cnecin();

    Super((void *)OldSP);  			      			// user mode 

	return 0;
}

void updateCounts(uint8_t doingWriteNotRead, int res)
{
    counts.singleOps++;

    // get the pointer to the right structure
    Tresults *pResults = doingWriteNotRead ? &counts.write : &counts.read;
    
  	switch( res )
	{
		case 0:                                 // test succeeded
            if(hdIf.retriesDoneCount == 0) {    // success without retries
                pResults->goodPlain++;
            } else {                            // success with some retries
                pResults->goodWithRetry++;
            }
            break;
        //---------------
        case -1:                                // test failed with communication error
            pResults->errorTimeout++;
            break;
        //---------------
        case -2:                                // test failed with CRC error
            pResults->errorCrc++;
            break;
        //---------------
        default:                                // some other error?
            pResults->errorOther++;
            break;
    }    
}

void printOpResult(int res) 
{
  	switch( res )
	{
		case 0:                                 // test succeeded
            if(hdIf.retriesDoneCount == 0) {    // success without retries
                Cconout('*');
            } else {                            // success with some retries
                char retryCountChar = (hdIf.retriesDoneCount < 9) ? ('0' + hdIf.retriesDoneCount) : '9';
                Cconout(retryCountChar);
            }
            break;
        //---------------
        case -1:                                // test failed with communication error
            Cconout('_');
            break;
        //---------------
        case -2:                                // test failed with CRC error
            Cconout('c');
            break;
        //---------------
        default:                                // some other error?
            Cconout('?');
            break;
    }    
}

void print_head()
{
  	VT52_Goto_pos(0,0);

	(void) Cconws("\33p[ CosmosEx IKBD test  ver "); 
    showAppVersion();
    (void) Cconws(" ]\33q\r\n"); 		
}

void print_status(void)
{
    char colorOrBw = 0;

    //-------------
    // 2nd row - general info
	VT52_Goto_pos(0,1);
	(void) Cconws("\33p[ Run:");
	showInt(counts.run, 3);
	
    (void) Cconws("  Ops: ");
	showInt(counts.singleOps, 5);
    
    (void) Cconws("  Data: ");

    // calculate and show transfered data capacity
    uint32_t kilobytes     = 127 * counts.singleOps;       // 127 kB per operation * operations count
    int megsInteger     = kilobytes / 1024;             // get integer part of mega_bytes
    int megsFraction    = (kilobytes % 1024) / 100;     // get mega_bytes_after_decimal_point part
    
    showIntWithPrepadding(megsInteger, 4, ' ');
    Cconout('.');
    showInt(megsFraction, 1);
	(void) Cconws(" MB ]\33q\r\n");

    //-------------
    // 3rd row - read statistics
    if(colorOrBw) {
        (void) Cconws("\33p[ R \33c2Gp:");
    } else {
        (void) Cconws("\33p[ R Gp:");
    }
        
	showInt(counts.read.goodPlain,      4);
    
	(void) Cconws(" Gr:");
	showInt(counts.read.goodWithRetry,  3);
    
    if(colorOrBw) {
        (void) Cconws(" \33c1T/O:");
    } else {
        (void) Cconws(" T/O:");
    }
    
	showInt(counts.read.errorTimeout,   2);

	(void) Cconws(" CRC:");
	showInt(counts.read.errorCrc,       2);

	(void) Cconws(" Oth:");
	showInt(counts.read.errorOther,     2);

    if(colorOrBw) {
        (void) Cconws("\33c0]\33q\r\n");
    } else {
        (void) Cconws("]\33q\r\n");
    }
    
    //-------------
    // 4rd row - write statistics
    if(colorOrBw) {
        (void) Cconws("\33p[ W \33c2Gp:");
    } else {
        (void) Cconws("\33p[ W Gp:");
    }
    
	showInt(counts.write.goodPlain,     4);
    
	(void) Cconws(" Gr:");
	showInt(counts.write.goodWithRetry, 3);
    
    if(colorOrBw) {
        (void) Cconws(" \33c1T/O:");
    } else {
        (void) Cconws(" T/O:");
    }
	showInt(counts.write.errorTimeout,  2);

	(void) Cconws(" CRC:");
	showInt(counts.write.errorCrc,      2);

	(void) Cconws(" Oth:");
	showInt(counts.write.errorOther,    2);

    if(colorOrBw) {
        (void) Cconws("\33c0]\33q\r\n");
    } else {
        (void) Cconws("]\33q\r\n");
    }
}

//--------------------------------------------------
uint8_t ce_identify(uint8_t ACSI_id)
{
  uint8_t cmd[CMD_LENGTH_SHORT] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, TRAN_CMD_IDENTIFY, 0};
  
  cmd[0] = (ACSI_id << 5); 					// cmd[0] = ACSI_id + TEST UNIT READY (0)	
  memset(rBuffer, 0, 512);              	// clear the buffer 

  (*hdIf.cmd) (ACSI_READ, cmd, CMD_LENGTH_SHORT, rBuffer, 1);   // issue the identify command and check the result 
    
  if(!hdIf.success || hdIf.statusByte != 0) {                   // if failed, return FALSE 
    return 0;
  }
    
  if(strncmp((char *) rBuffer, "CosmosEx translated disk", 24) != 0) {		// the identity string doesn't match? 
	 return 0;
  }
	
  return 1;                             // success 
}
//--------------------------------------------------
void showConnectionErrorMessage(void)
{
//	Clear_home();
	(void) Cconws("Communication with CosmosEx failed.\nWill try to reconnect in a while.\n\nTo quit to desktop, press F10\n");
	
	prevCommandFailed = 1;
}
//--------------------------------------------------
uint8_t findDevice(void)
{
	uint8_t i;
	uint8_t key, res;
	uint8_t id = 0xff;
	char bfr[2];

    hdIf.maxRetriesCount = 0;           // disable retries - we are expecting that the devices won't answer on every ID
    
	bfr[1] = 0; 
	(void) Cconws("Looking for CosmosEx on ");
    
    switch(ifUsed) {
        case IF_ACSI:           (void) Cconws("ACSI: ");        break;
        case IF_SCSI_TT:        (void) Cconws("TT SCSI: ");     break;
        case IF_SCSI_FALCON:    (void) Cconws("Falcon SCSI: "); break;
    }

	while(1) {
		for(i=0; i<8; i++) {
			bfr[0] = i + '0';
			(void) Cconws(bfr); 
		      
			res = ce_identify(i);      					// try to read the IDENTITY string 
      
			if(res == 1) {                           	// if found the CosmosEx 
				id = i;                     		    // store the ACSI ID of device 
				break;
			}
		}
  
		if(res == 1) {                             		// if found, break 
			break;
		}
      
		(void) Cconws(" - not found.\r\nPress any key to retry or 'Q' to quit.\r\n");
		key = Cnecin();        
    
		if(key == 'Q' || key=='q') {
            hdIf.maxRetriesCount = 16;                  // enable retries
			return 0xff;
		}
	}
  
    hdIf.maxRetriesCount = 16;                          // enable retries
    
	bfr[0] = id + '0';
	(void) Cconws("\r\nCosmosEx ID: ");
	(void) Cconws(bfr);
	(void) Cconws("\r\n\r\n");
    
	return id;
}

//--------------------------------------------------

int readHansTest(uint32_t byteCount, uint16_t xorVal )
{
	commandLong[4+1] = TEST_READ;

    // size to read
	commandLong[5+1] = (byteCount >> 16) & 0xFF;
	commandLong[6+1] = (byteCount >>  8) & 0xFF;
	commandLong[7+1] = (byteCount      ) & 0xFF;

    // uint16_t to XOR with data on CE side
	commandLong[8+1] = (xorVal >> 8) & 0xFF;
	commandLong[9+1] = (xorVal     ) & 0xFF;

    memset(rBuffer, 0, 8);                  // clear first few bytes so we can detect if the data was really read, or it was only retained from last write in the buffer    
        
	(*hdIf.cmd) (ACSI_READ, commandLong, CMD_LENGTH_LONG, rBuffer, (byteCount+511)>>9 );		// issue the command and check the result
    
    if(!hdIf.success) {                     // FAIL? quit...
        return -1;
    }
    
    // if we came here, then either there's no error, or there's error but we still want to compare the buffers
    uint8_t retVal;
    if(hdIf.success && hdIf.statusByte == OK) { // no error - at the end just return 0
        retVal = 0;
    } else {            // some error - at the end return -1
        retVal = -1;
    }
    
    uint16_t counter = 0;
    uint16_t data = 0;
    uint32_t i;
    for(i=0; i<byteCount; i += 2) {
        data = counter ^ xorVal;       // create uint16_t
        if( !(rBuffer[i]==(data>>8) && rBuffer[i+1]==(data&0xFF)) ){
            return -2;
        }  
        counter++;
    }

    if(byteCount & 1) {                                 // odd number of bytes? add last byte
        uint8_t lastByte = (counter ^ xorVal) >> 8;
        if( rBuffer[byteCount-1]!=lastByte ){
            return -2;
        }  
    }
    
    if(retVal != 0) {       // if we came here, then either there's no xCSI error, or there was xCSI error but the received buffer is still OK
        logMsg("xCSI cmd failed, but the READ buffer seems to be OK, wtf?!");
    }
    
	return retVal;
}

//--------------------------------------------------

int writeHansTest(uint32_t byteCount, uint16_t xorVal)
{
    static uint16_t prevXorVal = 0xffff;
    
	commandLong[4+1] = TEST_WRITE;

  //size to read
	commandLong[5+1] = (byteCount >> 16) & 0xFF;
	commandLong[6+1] = (byteCount >> 8 ) & 0xFF;
	commandLong[7+1] = (byteCount      ) & 0xFF;

  //uint16_t to XOR with data on CE side
	commandLong[8+1] = (xorVal >> 8) & 0xFF;
	commandLong[9+1] = (xorVal     ) & 0xFF;

    if(prevXorVal != xorVal) {              // if xorVal changed since last call, generate buffer (otherwise skip that)
        prevXorVal = xorVal;
    
        uint16_t counter = 0;
        uint16_t data = 0;
        uint32_t i;
        for(i=0; i<byteCount; i += 2) {
            data = counter ^ xorVal;       // create uint16_t
            wBuffer[i  ]    = (data >> 8);
            wBuffer[i+1]    = (data &  0xFF);
            counter++;
        }

        if(byteCount & 1) {                                 // odd number of bytes? add last byte
            uint8_t lastByte        = (counter ^ xorVal) >> 8;
            wBuffer[byteCount-1]    = lastByte;
        }
    }

	(*hdIf.cmd) (ACSI_WRITE, commandLong, CMD_LENGTH_LONG, wBuffer, (byteCount+511)>>9 );		// issue the command and check the result
    
    if(!hdIf.success) {                 // fail?
logMsg("writeHansTest: not success");
        return -1;
    }
    
    if(hdIf.statusByte == E_CRC) {      // status byte: CRC error?
logMsg("writeHansTest: E_CRC");
        return -2;
    }
    
    if(hdIf.statusByte != 0) {          // some other error?
logMsg("writeHansTest: status byte is ");
showHexByte(hdIf.statusByte);
        return -3;
    }
    
	return 0;
}

void deleteErrorLines(void)
{
    int line;
    
    for(line=ERROR_LINE_START; line<errorLine; line++) {
        VT52_Goto_pos(0, line);
        (void) Cconws("                                        ");
    }
}

void speedTest(void)
{
    uint32_t byteCount = ((uint32_t) MAXSECTORS) << 9;     // convert sector count to byte count ( sc * 512 )

	commandLong[4+1] = TEST_READ;

    // size to read
	commandLong[5+1] = (byteCount >> 16) & 0xFF;
	commandLong[6+1] = (byteCount >>  8) & 0xFF;
	commandLong[7+1] = (byteCount      ) & 0xFF;

    // uint16_t to XOR with data on CE side
	commandLong[8+1] = 0;
	commandLong[9+1] = 0;

  	VT52_Goto_pos(0, 23);
    (void) Cconws("Read speed: ");
    
  	uint32_t now, until, diff;
	now = *HZ_200;
    
    int i;
    
    for(i=0; i<20; i++) {
        (*hdIf.cmd) (ACSI_READ, commandLong, CMD_LENGTH_LONG, rBuffer, MAXSECTORS );  // issue the command and check the result

        if(!hdIf.success) {                     // ACSI ERROR?
            (void) Cconws("fail -- on ");
            showInt(i, 2);
            (void) Cconws("out of ");
            showInt(20, 2);
            (void) Cconws("\n\r");

            (void) Cnecin();
            return;
        }
    }
    
  	until   = *HZ_200;
    diff    = until - now;

    int timeMs  = (diff * 1000) / 200;
    int kbps    = ((20 * MAXSECTORS) * 500) / timeMs;
    
    showInt(kbps, -1);
    (void) Cconws(" kB/s\n\r");
    
    (void) Cconws("Press any key to continue...\n\r");
    (void) Cnecin();
}
