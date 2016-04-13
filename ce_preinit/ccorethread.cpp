#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <net/if.h>

#include "global.h"
#include "debug.h"
#include "ccorethread.h"
#include "gpio.h"

extern THwConfig    hwConfig;
extern TFlags       flags;

extern DWORD appStartMs;

CCoreThread::CCoreThread(void)
{
    setEnabledIDbits        = false;

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));
    
	conSpi		= new CConSpi();

    dataTrans   = new AcsiDataTrans();
    dataTrans->setCommunicationObject(conSpi);
}

CCoreThread::~CCoreThread()
{
    delete conSpi;
    delete dataTrans;
}

void CCoreThread::run(void)
{
    BYTE inBuff[8], outBuf[8];

	memset(outBuf, 0, 8);
    memset(inBuff, 0, 8);

    loadSettings();

    //------------------------------
    // stuff related to checking of Hans being alive
    flags.gotHansFwVersion  = false;
    Utils::resetHansAndFranz();
    Utils::franzToResetState();     // keep Franz in RESET state

    struct {
        DWORD hans;
        DWORD franz;
        DWORD nextDisplay;
        
        DWORD hansResetTime;
        DWORD franzResetTime;
        
        int     progress;
    } lastFwInfoTime;
    char progChars[4] = {'|', '/', '-', '\\'};
    
    lastFwInfoTime.hans         = 0;
    lastFwInfoTime.franz        = 0;
    lastFwInfoTime.nextDisplay  = Utils::getEndTime(1000);
    lastFwInfoTime.progress     = 0;
    
    lastFwInfoTime.hansResetTime    = Utils::getCurrentMs();

	bool res;
    
    LoadTracker load;

    while(sigintReceived == 0) {
		bool gotAtn = false;						                    // no ATN received yet?
        
        DWORD now = Utils::getCurrentMs();
        if(now >= lastFwInfoTime.nextDisplay) {
            lastFwInfoTime.nextDisplay  = Utils::getEndTime(1000);
            
            //-------------
            // calculate load, show message if needed
            load.calculate();                                           // calculate load
            
            if(load.suspicious) {                                       // load is suspiciously high?
                Debug::out(LOG_DEBUG, ">>> Suspicious core cycle load -- cycle time: %4d ms, load: %3d %%", load.cycle.total, load.loadPercents);
                printf(">>> Suspicious core cycle load -- cycle time: %4d ms, load: %3d %%\n", load.cycle.total, load.loadPercents);

                lastFwInfoTime.hansResetTime    = now;
                }
            //-------------
            
            float hansTime  = ((float)(now - lastFwInfoTime.hans))  / 1000.0f;
            
            hansTime        = (hansTime  < 15.0f) ? hansTime  : 15.0f;
            bool hansAlive  = (hansTime < 3.0f);
            
            printf("\033[2K  [ %c ]  Hans: %s\033[A\n", progChars[lastFwInfoTime.progress], hansAlive ? "LIVE" : "DEAD");
        
            lastFwInfoTime.progress = (lastFwInfoTime.progress + 1) % 4;
            
            if(!hansAlive && (now - lastFwInfoTime.hansResetTime) >= 1500) {
                printf("\033[2KHans not alive, resetting Hans.\n");
                Debug::out(LOG_INFO, "Hans not alive, resetting Hans.");
                lastFwInfoTime.hansResetTime = now;
                Utils::resetHans();
            }

            load.clear();                       // clear load counter
        }
        
        load.busy.markStart();                          // mark the start of the busy part of the code
        
        // check for any ATN code waiting from Hans
		res = conSpi->waitForATN(SPI_CS_HANS, (BYTE) ATN_ANY, 0, inBuff);

		if(res) {									    // HANS is signaling attention?
			gotAtn = true;							    // we've some ATN

			switch(inBuff[3]) {
			case ATN_FW_VERSION:
                    lastFwInfoTime.hans = Utils::getCurrentMs();
                    handleFwVersion_hans();
				break;

			case ATN_ACSI_COMMAND:
                    handleAcsiCommand();
				break;

			default:
				Debug::out(LOG_ERROR, (char *) "CCoreThread received weird ATN code %02x waitForATN()", inBuff[3]);
				break;
			}
		}

        load.busy.markEnd();                        // mark the end of the busy part of the code

		if(!gotAtn) {								// no ATN was processed?
			Utils::sleepMs(1);						// wait 1 ms...
		}
    }
}

void CCoreThread::handleAcsiCommand(void)
{
    BYTE bufOut[ACSI_CMD_SIZE];
    BYTE bufIn[ACSI_CMD_SIZE];

    memset(bufOut,  0, ACSI_CMD_SIZE);
    memset(bufIn,   0, ACSI_CMD_SIZE);

    conSpi->txRx(SPI_CS_HANS, 14, bufOut, bufIn);           // get 14 cmd bytes

    #define E_CHECK_CONDITION   0x02
    #define E_WAITING_FOR_MOUNT 0x7e

    BYTE acsiId = bufIn[0] >> 5;                        	// get just ACSI ID
    if(acsiIdInfo.acsiIDdevType[acsiId] == DEVTYPE_OFF) {	// if this ACSI ID is off, reply with error and quit
        dataTrans->setStatus(E_CHECK_CONDITION);
        dataTrans->sendDataAndStatus();
        return;
    }

    dataTrans->setStatus(E_WAITING_FOR_MOUNT);
    dataTrans->sendDataAndStatus();
    return;
}

void CCoreThread::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "CCoreThread::loadSettings");

    Settings s;
	s.loadAcsiIDs(&acsiIdInfo);

    setEnabledIDbits    = true;

    //-----------
    // show config to console and log
    char tmp[32];
    int i;
    
    for(i = 0; i < 8; i++) {
        if(acsiIdInfo.acsiIDdevType[i] != DEVTYPE_OFF) {
            tmp[7 - i] = '0' + i;
        } else {
            tmp[7 - i] = '-';
        }
    }
    tmp[8] = 0;
    
    Debug::out(LOG_INFO, "CCoreThread::loadSettings - enabled IDs: %s", tmp);
    printf("Enabled IDs: %s\n", tmp);
    
    //-----------
    for(i = 0; i < 8; i++) {
        if(acsiIdInfo.acsiIDdevType[i] != DEVTYPE_OFF) {
            switch(acsiIdInfo.acsiIDdevType[i]) {
            case DEVTYPE_SD:            printf("[%d] SD card\n", i);        break;
            case DEVTYPE_RAW:           printf("[%d] RAW USB\n", i);        break;
            case DEVTYPE_TRANSLATED:    printf("[%d] CE_DD driver\n", i);   break;
            }
        }
    }
}

#define MAKEWORD(A, B)  ( (((WORD)A)<<8) | ((WORD)B) )

void CCoreThread::handleFwVersion_hans(void)
{
    BYTE fwVer[14], oBuf[14];
	int cmdLength;

    memset(oBuf,    0, 14);                                         // first clear the output buffer
    memset(fwVer,   0, 14);

    // WORD sent (bytes shown): 01 23 45 67

    cmdLength = 12;
    responseStart(cmdLength);                                       // init the response struct

    //--------------
    // send the ACSI + SD config + FDD enabled images, when they changed from current values to something new
    BYTE enabledIDbits, sdCardAcsiId;
    getIdBits(enabledIDbits, sdCardAcsiId);                                 // get the enabled IDs 
    
    hansConfigWords.next.acsi   = MAKEWORD(enabledIDbits,                   sdCardAcsiId);
    hansConfigWords.next.fdd    = 0;
    
    if(hansConfigWords.next.acsi  != hansConfigWords.current.acsi) {
        // hansConfigWords.skipNextSet - it's a flag used for skipping one config sending, because we send the new config now, but receive it processed in the next (not this) fw version packet
        
        if(!hansConfigWords.skipNextSet) {                      
            DWORD now   = Utils::getCurrentMs();
            int   diff  = now - appStartMs;
    
            Debug::out(LOG_INFO, "Sending IDs config %d ms after app start", diff);
            printf(              "Sending IDs config %d ms after app start\n", diff);
        
            responseAddWord(oBuf, CMD_ACSI_CONFIG);             // CMD: send acsi config
            responseAddWord(oBuf, hansConfigWords.next.acsi);   // store ACSI enabled IDs and which ACSI ID is used for SD card
            responseAddWord(oBuf, 0);                           // store which floppy images are enabled
            
            hansConfigWords.skipNextSet = true;                 // we have just sent the config, skip the next sending, so we won't send it twice in a row
        } else {                                                // if we should skip sending config this time, then don't skip it next time (if needed)
            hansConfigWords.skipNextSet = false;
        }
    }
    //--------------

    conSpi->txRx(SPI_CS_HANS, cmdLength, oBuf, fwVer);

    int year = bcdToInt(fwVer[1]) + 2000;

    flags.gotHansFwVersion = true;

    int  currentLed = fwVer[4];
    BYTE xilinxInfo = fwVer[5];

    hansConfigWords.current.acsi    = MAKEWORD(fwVer[6], fwVer[7]);
    hansConfigWords.current.fdd     = MAKEWORD(fwVer[8],        0);
    
    convertXilinxInfo(xilinxInfo);
    
    Debug::out(LOG_DEBUG, "FW: Hans,  %d-%02d-%02d, LED is: %d, XI: 0x%02x", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]), currentLed, xilinxInfo);

    // if Xilinx HW vs FW mismatching, flash Xilinx again to fix the situation
    if(hwConfig.fwMismatch) {
        Debug::out(LOG_ERROR, ">>> Terminating app, because there's Xilinx HW vs FW mismatch! <<<\n");
        sigintReceived = 1;
        return;
    }
}

void CCoreThread::getIdBits(BYTE &enabledIDbits, BYTE &sdCardAcsiId)
{
    // get the bits from struct
    enabledIDbits  = acsiIdInfo.enabledIDbits;
    sdCardAcsiId   = acsiIdInfo.sdCardAcsiId;
    
    if(hwConfig.hddIface != HDD_IF_SCSI) {          // not SCSI? Don't change anything
//        Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on ACSI");
        return;
    }
    
    // if we're on SCSI bus, remove ID bits if they are used for SCSI Initiator on that machine (ID 7 on TT, ID 0 on Falcon)
    switch(hwConfig.scsiMachine) {
        case SCSI_MACHINE_TT:                       // TT? remove bit 7 
//            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on TT, will remove ID 7 from enabled ID bits");
        
            enabledIDbits = enabledIDbits & 0x7F;
            if(sdCardAcsiId == 7) {
                sdCardAcsiId = 0xff;
            }
            break;

        //------------
        case SCSI_MACHINE_FALCON:                   // Falcon? remove bit 0
//            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on Falcon, will remove ID 0 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0xFE;
            if(sdCardAcsiId == 0) {
                sdCardAcsiId = 0xff;
            }
            break;

        //------------
        default:
        case SCSI_MACHINE_UNKNOWN:                  // unknown machine? remove both bits 7 and 0
//            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on unknown machine, will remove ID 7 and ID 0 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0x7E;
            if(sdCardAcsiId == 0 || sdCardAcsiId == 7) {
                sdCardAcsiId = 0xff;
            }
            break;
    }
}

void CCoreThread::convertXilinxInfo(BYTE xilinxInfo)
{
    THwConfig   hwConfigOld     = hwConfig;
    int         prevHwHddIface  = hwConfig.hddIface; 
    
    switch(xilinxInfo) {
        // GOOD
        case 0x21:  hwConfig.version        = 2;                        // v.2
                    hwConfig.hddIface       = HDD_IF_ACSI;              // HDD int: ACSI
                    hwConfig.fwMismatch     = false;
                    break;

        // GOOD
        case 0x22:  hwConfig.version        = 2;                        // v.2
                    hwConfig.hddIface       = HDD_IF_SCSI;              // HDD int: SCSI
                    hwConfig.fwMismatch     = false;
                    break;

        // BAD: SCSI HW, ACSI FW
        case 0x29:  hwConfig.version        = 2;                        // v.2
                    hwConfig.hddIface       = HDD_IF_SCSI;              // HDD int: SCSI
                    hwConfig.fwMismatch     = true;                     // HW + FW mismatch!
                    break;

        // BAD: ACSI HW, SCSI FW
        case 0x2a:  hwConfig.version        = 2;                        // v.2
                    hwConfig.hddIface       = HDD_IF_ACSI;              // HDD int: ACSI
                    hwConfig.fwMismatch     = true;                     // HW + FW mismatch!
                    break;
                    
        // GOOD
        case 0x11:  // use this for v.1 
        default:    // and also for all other cases
                    hwConfig.version        = 1;
                    hwConfig.hddIface       = HDD_IF_ACSI;
                    hwConfig.fwMismatch     = false;
                    break;
    }
    
    // if the HD IF changed (received the 1st HW info) and we're on SCSI bus, we need to send the new (limited) SCSI IDs to Hans, so he won't answer on Initiator SCSI ID
    if((prevHwHddIface != hwConfig.hddIface) && hwConfig.hddIface == HDD_IF_SCSI) {
        Debug::out(LOG_DEBUG, "Found out that we're running on SCSI bus - will resend the ID bits configuration to Hans");
        setEnabledIDbits = true;
    }
    
    if(memcmp(&hwConfigOld, &hwConfig, sizeof(THwConfig)) != 0) {    // config changed? save it
        saveHwConfig();
    }

    //----------
    static bool firstRun = true;
    
    if(firstRun) {
        firstRun = false;
        
        if(hwConfig.fwMismatch) {
            printf("HDD IF: mismatch!\n");
        } else {
            printf("HDD IF: %s\n", (hwConfig.hddIface == HDD_IF_ACSI) ? "ACSI" : "SCSI");
        }
    }
}

void CCoreThread::saveHwConfig(void)
{
    Settings s;
    
    int ver, hddIf, scsiMch;
    
    // get current values for these configs
    ver     = s.getInt((char *) "HW_VERSION",       1);
    hddIf   = s.getInt((char *) "HW_HDD_IFACE",     HDD_IF_ACSI);
    scsiMch = s.getInt((char *) "HW_SCSI_MACHINE",  SCSI_MACHINE_UNKNOWN);

    // store value only if it has changed
    if(ver != hwConfig.version) {
        s.setInt((char *) "HW_VERSION", ver);
    }
    
    if(hddIf != hwConfig.hddIface) {
        s.setInt((char *) "HW_HDD_IFACE", hddIf);
    }
    
    if(scsiMch != hwConfig.scsiMachine) {
        s.setInt((char *) "HW_SCSI_MACHINE", scsiMch);
    }
}

void CCoreThread::responseStart(int bufferLengthInBytes)        // use this to start creating response (commands) to Hans or Franz
{
    response.bfrLengthInBytes   = bufferLengthInBytes;
    response.currentLength      = 0;
}

void CCoreThread::responseAddWord(BYTE *bfr, WORD value)        // add a WORD to the response (command) to Hans or Franz
{
    if(response.currentLength >= response.bfrLengthInBytes) {
        return;
    }

    bfr[response.currentLength + 0] = (BYTE) (value >> 8);
    bfr[response.currentLength + 1] = (BYTE) (value & 0xff);
    response.currentLength += 2;
}

void CCoreThread::responseAddByte(BYTE *bfr, BYTE value)        // add a BYTE to the response (command) to Hans or Franz
{
    if(response.currentLength >= response.bfrLengthInBytes) {
        return;
    }

    bfr[response.currentLength] = value;
    response.currentLength++;
}

int CCoreThread::bcdToInt(int bcd)
{
    int a,b;

    a = bcd >> 4;       // upper nibble
    b = bcd &  0x0f;    // lower nibble

    return ((a * 10) + b);
}

