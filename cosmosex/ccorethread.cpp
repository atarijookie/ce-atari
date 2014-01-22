#include <unistd.h>
#include <string.h>

#include "global.h"
#include "debug.h"
#include "ccorethread.h"
#include "native/scsi_defs.h"
#include "settings.h"
#include "gpio.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define LOGFILE     "H:/acsilog.txt"
#define MEDIAFILE   "C:/datamedia.img"

#define DEV_CHECK_TIME_MS	3000

CCoreThread::CCoreThread()
{
    setEnabledIDbits = false;

    sendSingleHalfWord = false;

	conSpi		= new CConSpi();

    dataTrans   = new AcsiDataTrans();
    dataTrans->setCommunicationObject(conSpi);

    scsi        = new Scsi();
    scsi->setAcsiDataTrans(dataTrans);

    scsi->attachToHostPath(MEDIAFILE, SOURCETYPE_IMAGE_TRANSLATEDBOOT, SCSI_ACCESSTYPE_FULL);

    translated = new TranslatedDisk();
    translated->setAcsiDataTrans(dataTrans);

    translated->attachToHostPath("H:\\dom", TRANSLATEDTYPE_NORMAL);

    confStream = new ConfigStream();
    confStream->setAcsiDataTrans(dataTrans);

    // now register all the objects which use some settings in the proxy
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_ACSI);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) scsi,          SETTINGSUSER_ACSI);
	settingsReloadProxy.addSettingsUser((ISettingsUser *) translated,    SETTINGSUSER_TRANSLATED);
}

CCoreThread::~CCoreThread()
{
    delete conSpi;
    delete dataTrans;
    delete scsi;

    delete translated;
    delete confStream;
}

void CCoreThread::run(void)
{
    BYTE inBuff[8], outBuf[8];
	
	memset(outBuf, 0, 8);
    memset(inBuff, 0, 8);
	
    loadSettings();

	DWORD nextDevFindTime = Utils::getCurrentMs();	// create a time when the devices should be checked - and that time is now

	bool res;
	
    while(1) {
		bool gotAtn = false;						// no ATN received yet?
		
		if(Utils::getCurrentMs() >= nextDevFindTime) {	// should we check for the new devices?
			devFinder.lookForDevChanges();				// look for devices attached / detached
			
			nextDevFindTime = Utils::getEndTime(DEV_CHECK_TIME_MS);		// update the time when devices should be checked
		}

		res = conSpi->waitForATN(SPI_CS_HANS, ATN_ANY, 0, inBuff);		// check for any ATN code waiting from Hans

		if(res) {									// HANS is signaling attention?
			gotAtn = true;							// we've some ATN

			switch(inBuff[3]) {
			case 0:                 				// this is valid, just empty data, skip this
				break;

			case ATN_FW_VERSION:
				handleFwVersion();
				break;

			case ATN_ACSI_COMMAND:
				handleAcsiCommand();
				break;

			default:
				logToFile((char *) "That ^^^ shouldn't happen!\n");
				break;
			}
		}
		
		res = conSpi->waitForATN(SPI_CS_FRANZ, ATN_ANY, 0, inBuff);		// check for any ATN code waiting from Franz
		if(res) {									// FRANZ is signaling attention?
			gotAtn = true;							// we've some ATN

			
		}
		
		if(!gotAtn) {								// no ATN was processed?
			Utils::sleepMs(1);						// wait 1 ms...
		}		
    }
}

void CCoreThread::handleAcsiCommand(void)
{
    #define CMD_SIZE    14

    BYTE bufOut[CMD_SIZE], bufIn[CMD_SIZE];
    memset(bufOut, 0, CMD_SIZE);

    conSpi->txRx(SPI_CS_HANS, 14, bufOut, bufIn);        // get 14 cmd bytes
    Debug::out("\nhandleAcsiCommand: %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5]);

    BYTE justCmd = bufIn[0] & 0x1f;
    BYTE wasHandled = false;

    BYTE acsiId = bufIn[0] >> 5;                        // get just ACSI ID

    if(acsiIDdevType[acsiId] == DEVTYPE_OFF) {          // if this ACSI ID is off, reply with error and quit
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        dataTrans->sendDataAndStatus();
        return;
    }

    // ok, so the ID is right, let's see what we can do
    if(justCmd == 0) {                              // if the command is 0 (TEST UNIT READY)
        if(bufIn[1] == 'C' && bufIn[2] == 'E') {    // and this is CosmosEx specific command

            switch(bufIn[3]) {
            case HOSTMOD_CONFIG:                    // config console command?
                wasHandled = true;
                confStream->processCommand(bufIn);
                break;

            case HOSTMOD_TRANSLATED_DISK:
                wasHandled = true;
                translated->processCommand(bufIn);
                break;
            }
        }
    } else if(justCmd == 0x1f) {                    // if the command is ICD mark
        BYTE justCmd2 = bufIn[1] & 0x1f;

        if(justCmd2 == 0 && bufIn[2] == 'C' && bufIn[3] == 'E') {    // the command is 0 (TEST UNIT READY), and this is CosmosEx specific command

            switch(bufIn[4]) {
            case HOSTMOD_TRANSLATED_DISK:
                wasHandled = true;
                translated->processCommand(bufIn + 1);
                break;
            }
        }
    }

    if(wasHandled != true) {                        // if the command was not previously handled, it's probably just some SCSI command
        scsi->processCommand(bufIn);                // process the command
    }
}

void CCoreThread::reloadSettings(void)
{
    loadSettings();
}

void CCoreThread::loadSettings(void)
{
    Debug::out("CCoreThread::loadSettings");

    Settings s;
    enabledIDbits = 0;                                    // no bits / IDs enabled yet

    char key[32];
    for(int id=0; id<8; id++) {							// read the list of device types from settings
        sprintf(key, "ACSI_DEVTYPE_%d", id);			// create settings KEY, e.g. ACSI_DEVTYPE_0
        int devType = s.getInt(key, DEVTYPE_OFF);

        if(devType < 0) {
            devType = DEVTYPE_OFF;
        }

        acsiIDdevType[id] = devType;

        if(devType != DEVTYPE_OFF) {                    // if ON
            enabledIDbits |= (1 << id);                 // set the bit to 1
        }
    }

    setEnabledIDbits = true;
}

void CCoreThread::handleFwVersion(void)
{
    BYTE fwVer[10], oBuf[10];

    memset(oBuf, 0, 10);                    // first clear the output buffer

    if(setEnabledIDbits) {                  // if we should send ACSI ID configuration
        oBuf[3] = CMD_ACSI_CONFIG;          // CMD: send acsi config
        oBuf[4] = enabledIDbits;            // store ACSI enabled IDs
        setEnabledIDbits = false;           // and don't sent this anymore (until needed)
    }

    conSpi->txRx(SPI_CS_HANS, 10, oBuf, fwVer);

    logToFile((char *) "handleFwVersion: \nOUT:\n");
    logToFile(10, oBuf);
    logToFile((char *) "\nIN:\n");
    logToFile(10, fwVer);
    logToFile((char *) "\n");

    int year = bcdToInt(fwVer[1]) + 2000;
    if(fwVer[0] == 0xf0) {
        Debug::out("FW: Franz, %d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));
    } else {
        int currentLed = fwVer[4];
        Debug::out("FW: Hans,  %d-%02d-%02d, LED is: %d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]), currentLed);
    }
}

int CCoreThread::bcdToInt(int bcd)
{
    int a,b;

    a = bcd >> 4;       // upper nibble
    b = bcd &  0x0f;    // lower nibble

    return ((a * 10) + b);
}

void CCoreThread::logToFile(char *str)
{
    FILE *f = fopen(LOGFILE, "at");

    if(!f) {
        return;
    }

    fprintf(f, str);
    fclose(f);
}

void CCoreThread::logToFile(WORD wval)
{
    FILE *f = fopen(LOGFILE, "at");

    if(!f) {
        return;
    }

//    fprintf(f, "%04x\n", wval);
    fprintf(f, "%d\n", wval);
    fclose(f);
}

void CCoreThread::logToFile(int len, BYTE *bfr)
{
    FILE *f = fopen(LOGFILE, "at");

    if(!f) {
        return;
    }

    fprintf(f, "buffer -- %d bytes\n", len);

    while(len > 0) {
        for(int col=0; col<16; col++) {
            if(len == 0) {
                break;
            }

            fprintf(f, "%02x ", *bfr);

            bfr++;
            len--;
        }
        fprintf(f, "\n");
    }

    fclose(f);
}
