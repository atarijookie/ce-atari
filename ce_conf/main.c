//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <mint/linea.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "acsi.h"
#include "hdd_if.h"
#include "keys.h"
#include "global.h"
#include "find_ce.h"
#include "vt52.h"

//--------------------------------------------------
void hdIfCmdAsUser(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

void showHomeScreen(void);
void sendKeyDown(BYTE key, BYTE keyDownCommand);
void refreshScreen(void);
BYTE setResolution(void);
void showConnectionErrorMessage(void);
void showMoreStreamIfNeeded(void);
int getAndShowAvailableApps(void);
void connectToAppIndex(uint8_t newAppIndex);

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key, int, int);
BYTE ce_identify(BYTE ACSI_id);

BYTE retrieveIsUpdating    (void);
void retrieveIsUpdateScreen(char *stream);
BYTE isUpdateScreen;
BYTE ceIsUpdating;

#define UPDATECOMPONENT_APP     0x01
#define UPDATECOMPONENT_XILINX  0x02
#define UPDATECOMPONENT_HANS    0x04
#define UPDATECOMPONENT_FRANZ   0x08
#define UPDATECOMPONENT_ALL     0x0f

BYTE updateComponents;

BYTE getKeyIfPossible(void);
void showFakeProgress(void);

void cosmoSoloConfig(void);
//--------------------------------------------------
BYTE deviceID;                          // bus ID from 0 to 7
BYTE cosmosExNotCosmoSolo;              // 0 means CosmoSolo, 1 means CosmosEx
//--------------------------------------------------

#define BUFFER_SIZE         (4*512)
BYTE myBuffer[BUFFER_SIZE];
BYTE *pBuffer;

BYTE prevCommandFailed;

//--------------------------------------------------
int main(void)
{
    BYTE key, res;
    DWORD toEven;
    BYTE keyDownCommand = CFG_CMD_KEYDOWN;
    DWORD lastUpdateCheckTime = 0;

    ceIsUpdating        = FALSE;
    isUpdateScreen      = FALSE;
    updateComponents    = 0;
    
    prevCommandFailed = 0;
    
    // ---------------------- 
    // create buffer pointer to even address 
    toEven = (DWORD) &myBuffer[0];
  
    if(toEven & 0x0001)         // not even number? 
        toEven++;
  
    pBuffer = (BYTE *) toEven; 
    
    // ---------------------- 
    // search for device on the ACSI / SCSI bus 
    deviceID = 0;

    Clear_home();
    res = Supexec(findDevice);

    if(res != TRUE) {
        return 0;
    }
    
    //------------------
    // if the device is CosmoSolo, go this way
    if(cosmosExNotCosmoSolo == FALSE) {
        cosmoSoloConfig();
        return 0;
    }
    
    // ----------------- 
    // if the device is CosmosEx, do the remote console config
    hdIf.maxRetriesCount = 1;                                       // retry only once

    if(getAndShowAvailableApps() == FALSE) {                        // show available apps, let user choose. If should quit, quit
        return 0;
    }

    setResolution();                                                // send the current ST resolution for screen centering 
    showHomeScreen();                                               // get the home screen 
    
    DWORD lastShowStreamTime = getTicksAsUser();                    // when was the last time when we got some config stream?
    
    while(1) {
        if(isUpdateScreen) {
            DWORD now = getTicksAsUser();
            
            if(now >= (lastUpdateCheckTime + 200)) {                // if last check was at least a second ago, do new check
                lastUpdateCheckTime = now;
                retrieveIsUpdating();                               // talk to CE to see if the update started
            }
        }
        
        if(ceIsUpdating) {                                          // if we're updating, show fake update progress
            showFakeProgress();
            ceIsUpdating = FALSE;                                   // we're (probably) not updating anymore
        }
    
        key = getKeyIfPossible();                                   // see if there's something waiting from keyboard 

        if(key == 0) {                                              // nothing waiting from keyboard? 
            DWORD now   = getTicksAsUser();
            DWORD diff  = now - lastShowStreamTime;
            
            if(diff > 200) {                  // last time the stream was shown was (at least) one second ago? do refresh...
                sendKeyDown(0, keyDownCommand);                     // display a new stream (if something changed) 

                lastShowStreamTime = now;                           // we just shown the stream, no need for refresh
            }

            continue;                                               // try again 
        }

        // switching between apps using Fx keys
        if(key >= KEY_F1 && key <= KEY_F9) {        // valid Fx key pressed
            int index = key - KEY_F1;               // transform key to value 0-8
            connectToAppIndex(index);               // switch to this app
            continue;
        }

        if(key == KEY_F10) {                                        // should quit? 
            break;
        }

        sendKeyDown(key, keyDownCommand);                           // send this key to device
        lastShowStreamTime = getTicksAsUser();                      // we just shown the stream, no need for refresh
    }
    
    return 0;
}
//--------------------------------------------------
void connectToAppIndex(uint8_t newAppIndex)
{
    // send command to CE to tell it which app (with newAppIndex) we want to use
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_SET_APP_INDEX, 0};
    cmd[0] = (deviceID << 5);                       // cmd[0] = ACSI_id + TEST UNIT READY (0)
    cmd[5] = newAppIndex;
    memset(pBuffer, 0, 512);                        // clear the buffer

    hdIfCmdAsUser(1, cmd, 6, pBuffer, 1);           // send command to ST

    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE
        showConnectionErrorMessage();
        return;
    }
}
//--------------------------------------------------
int getAndShowAvailableApps(void)
{
    // send command to CE, fetch list of apps
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_GET_APP_NAMES, 0};
    cmd[0] = (deviceID << 5);                       // cmd[0] = ACSI_id + TEST UNIT READY (0)
    memset(pBuffer, 0, 512);                        // clear the buffer

    hdIfCmdAsUser(1, cmd, 6, pBuffer, 1);           // send command to ST

    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE
        showConnectionErrorMessage();
        return FALSE;
    }

    // show list of apps with a message
    Clear_home();
    (void) Cconws("\33pList of available CosmosEx tools     \33q\n\r");
    (void) Cconws("\33pPress F1-F9 to select or F10 to quit.\33q\r\n\r\n");
    int i;
    for(i=0; i<9; i++) {                           // go through the list of apps
        char* pApp = (char*) pBuffer + i*40;        // 1 app has 40 chars for name, construct pointer to it

        if(*pApp != 0) {                            // this position not empty? show it
            (void) Cconws((char *) pApp);
            (void) Cconws("\r\n");
        }
    }

    // wait for a valid key
    while(1) {
        BYTE key = getKeyIfPossible();

        if(key >= KEY_F1 && key <= KEY_F9) {                // valid Fx key pressed
            int index = key - KEY_F1;                       // transform key to value 0-8
            char* pApp = (char*) pBuffer + index*40;        // 1 app has 40 chars for name, construct pointer to it

            if(*pApp != 0) {                                // this position not empty? use it!
                connectToAppIndex(index);                   // switch to this app
                break;
            }
        }

        if(key == KEY_F10 || key == 'q' || key == 'Q') {    // user requested quit by key press? quit
            return FALSE;
        }
    }

    return TRUE;
}
//--------------------------------------------------
void sendKeyDown(BYTE key, BYTE keyDownCommand)
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, keyDownCommand, 0};
    
    cmd[0] = (deviceID << 5);                       // cmd[0] = ACSI_id + TEST UNIT READY (0)   
    cmd[5] = key;                                   // store the pressed key to cmd[5] 
  
    memset(pBuffer, 0, 512);                        // clear the buffer 
  
    hdIfCmdAsUser(1, cmd, 6, pBuffer, 3);           // issue the KEYDOWN command and show the screen stream 
    
    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
        showConnectionErrorMessage();
        return;
    }
    
    if(prevCommandFailed != 0) {                    // if previous ACSI command failed, do some recovery 
        prevCommandFailed = 0;
        
        setResolution();
        showHomeScreen();
    }
    
    retrieveIsUpdateScreen((char *) pBuffer);       // get the flag isUpdateScreen from the end of the stream
    (void) Cconws((char *) pBuffer);                // now display the buffer
}
//--------------------------------------------------
void showHomeScreen(void)
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_GO_HOME, 0};
    
    cmd[0] = (deviceID << 5);                       // cmd[0] = ACSI_id + TEST UNIT READY (0)   
    memset(pBuffer, 0, 512);                        // clear the buffer 
  
    hdIfCmdAsUser(1, cmd, 6, pBuffer, 3);             // issue the GO_HOME command and show the screen stream 
    
    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
        showConnectionErrorMessage();
        return;
    }
    
    if(prevCommandFailed != 0) {                    // if previous ACSI command failed, do some recovery 
        prevCommandFailed = 0;
        
        setResolution();
        showHomeScreen();
    }
    
    retrieveIsUpdateScreen((char *) pBuffer);       // get the flag isUpdateScreen from the end of the stream
    (void) Cconws((char *) pBuffer);                // now display the buffer
}

BYTE showHomeScreenSimple(void)
{
    BYTE res = setResolution();                     // first try to set the new resolution
    
    if(res == FALSE) {                              // failed to set resolution? fail
        return FALSE;
    }

    // if we got here, the previous command passed and this should work also
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_GO_HOME, 0};
    
    cmd[0] = (deviceID << 5);                       // cmd[0] = ACSI_id + TEST UNIT READY (0)   
    memset(pBuffer, 0, 512);                        // clear the buffer 
  
    hdIfCmdAsUser(1, cmd, 6, pBuffer, 3);           // issue the GO_HOME command and show the screen stream 
    
    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
        return FALSE;
    }
    
    retrieveIsUpdateScreen((char *) pBuffer);       // get the flag isUpdateScreen from the end of the stream
    (void) Cconws((char *) pBuffer);                // now display the buffer
    
    return TRUE;                                    // return success
}
//--------------------------------------------------
void refreshScreen(void)                            
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_REFRESH, 0};
    
    cmd[0] = (deviceID << 5);                       // cmd[0] = ACSI_id + TEST UNIT READY (0)   
    memset(pBuffer, 0, 512);                        // clear the buffer 
  
    hdIfCmdAsUser(1, cmd, 6, pBuffer, 3);           // issue the REFRESH command and show the screen stream 
    
    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
        showConnectionErrorMessage();
        return;
    }
    
    retrieveIsUpdateScreen((char *) pBuffer);       // get the flag isUpdateScreen from the end of the stream
    (void) Cconws((char *) pBuffer);                // now display the buffer
}
//--------------------------------------------------
BYTE setResolution(void)                            
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_SET_RESOLUTION, 0};
    
    cmd[0] = (deviceID << 5);                       // cmd[0] = ACSI_id + TEST UNIT READY (0)   
    cmd[5] = Getrez();
    memset(pBuffer, 0, 512);                        // clear the buffer 
  
    hdIfCmdAsUser(1, cmd, 6, pBuffer, 1);           // issue the SET RESOLUTION command 
    
    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
        return FALSE;
    }

    return TRUE;                                    // if success, return TRUE
}
//--------------------------------------------------
void showConnectionErrorMessage(void)
{
    Clear_home();
    (void) Cconws("Link to device is down - updating?\n\rTrying to reconnect.\n\r\n\rTo quit to desktop, press F10\n\r");
    prevCommandFailed = 1;
}
//--------------------------------------------------
BYTE atariKeysToSingleByte(BYTE vkey, BYTE key, int shift, int ctrl)
{
    WORD vkeyKey;

    if(key >= 32 && key < 127) {        // printable ASCII key? just return it 
        return key;
    }
    
    if(key == 0) {                      // will this be some non-ASCII key? convert it 
        switch(vkey) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4b: return KEY_LEFT;
            case 0x4d: return KEY_RIGHT;
            case 0x52: return KEY_INSERT;
            case 0x47: return KEY_HOME;
            case 0x62: return KEY_HELP;
            case 0x61: return KEY_UNDO;
            case 0x3b: return KEY_F1;
            case 0x3c: return KEY_F2;
            case 0x3d: return KEY_F3;
            case 0x3e: return KEY_F4;
            case 0x3f: return KEY_F5;
            case 0x40: return KEY_F6;
            case 0x41: return KEY_F7;
            case 0x42: return KEY_F8;
            case 0x43: return KEY_F9;
            case 0x44: return KEY_F10;
            default: return 0;          // unknown key 
        }
    }
    
    vkeyKey = (((WORD) vkey) << 8) | ((WORD) key);      // create a WORD with vkey and key together 
    
    switch(vkeyKey) {                   // some other no-ASCII key, but check with vkey too 
        case 0x011b: return KEY_ESC;
        case 0x537f: return KEY_DELETE;
        case 0x0e08: return KEY_BACKSP;
        case 0x0f09: return shift ? KEY_SHIFT_TAB : KEY_TAB;
        case 0x1c0d: return KEY_ENTER;
        case 0x720d: return KEY_ENTER;
    }

    return 0;                           // unknown key 
}
//--------------------------------------------------
BYTE processUpdateComponentsFlags(BYTE inByteWithFlags)
{
    BYTE components     =  inByteWithFlags;     // get where the update components should be stored
    BYTE validitySign   = (components & 0xf0);  // get upper nibble
    
    BYTE upComponents   = 0;
    
    if(validitySign == 0xc0) {                  // if upper nible is 'C', then this valid update components thing
        upComponents = components & 0x0f;       // get only the bottom part of components byte
        
        if(upComponents == 0) {                 // no update components? pretend that we're updating everything to wait long enough
            upComponents = UPDATECOMPONENT_ALL;
        }
    } else {                                    // if update components is possibly invalid, pretend that we're updating everything to wait long enough
        upComponents = UPDATECOMPONENT_ALL;
    }
    
    return upComponents;
}
//--------------------------------------------------
BYTE retrieveIsUpdating(void)
{
    ceIsUpdating = FALSE;                           // not updating yet
    
    // if we got here, the previous command passed and this should work also
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_UPDATING_QUERY, 0};
    
    cmd[0] = (deviceID << 5);                       // cmd[0] = ACSI_id + TEST UNIT READY (0)   
    memset(pBuffer, 0, 512);                        // clear the buffer 
  
    hdIfCmdAsUser(1, cmd, 6, pBuffer, 1);           // issue UPDATING QUERY command
    
    if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
        return FALSE;
    }
    
    ceIsUpdating        = pBuffer[0];               // get the IS UPDATING flag
    updateComponents    = processUpdateComponentsFlags(pBuffer[1]);
    return TRUE;                                    // return success
}
//--------------------------------------------------
void retrieveIsUpdateScreen(char *stream)
{
    WORD i;
    
    for(i=0; i<BUFFER_SIZE; i++) {
        if(stream[i] == 0) {            // end of stream? good
            break;
        }
    }
    
    if(i == BUFFER_SIZE) {              // reached end of buffer? not update screen (possibly)
        isUpdateScreen = FALSE;
        return;
    }
    
    if(stream[i + 1] == 1) {            // if the flag after the stream is equal to 1, then it's update screen
        isUpdateScreen      = TRUE;
        updateComponents    = processUpdateComponentsFlags(stream[i + 2]);
    } else {                            // it's not update screen
        isUpdateScreen      = FALSE;
        updateComponents    = 0;        // no components will be updated
    }
}
//--------------------------------------------------
void cosmoSoloConfig(void)
{
    (void) Cconws("\n\rCurrent device ID is: ");
    Cconout('0' + deviceID);
    (void) Cconws("\n\rPlease enter new device ID (0 - 7)\n\ror any other key to quit.");
    (void) Cconws("\n\rEnter new device ID : ");
    
    BYTE key = Cnecin();
    
    if(key >= '0' && key <= '7') {
        BYTE newId = key - '0';
        BYTE cmd[] = {0, 'C', 'S', deviceID, newId, 0};
        
        cmd[0] = (deviceID << 5);                           // cmd[0] = ACSI_id + TEST UNIT READY (0)   
  
        hdIfCmdAsUser(ACSI_READ, cmd, 6, pBuffer, 1);
        
        if(!hdIf.success || hdIf.statusByte == OK) {
            (void) Cconws("\n\rNew ID was successfully set.\n\r");
        } else {
            (void) Cconws("\n\rFailed to set new ID.\n\r");
        }
    } else {
        (void) Cconws("\n\rTerminating without setting device ID.\n\r");
    }
    
    sleep(3);
}
//--------------------------------------------------
void logMsg(char *logMsg)
{
//    if(showLogs) {
//        (void) Cconws(logMsg);
//    }
}
//--------------------------------------------------
void logMsgProgress(DWORD current, DWORD total)
{
//    (void) Cconws("Progress: ");
//    showHexDword(current);
//    (void) Cconws(" out of ");
//    showHexDword(total);
//    (void) Cconws("\n\r");
}
//--------------------------------------------------
void showFakeProgressOfItem(const char *title, int timeout)
{
    (void) Cconws(title);               // show what we are updating

    int i, j;
    for(i=0; i<=timeout; i++) {         // wait the whole timeout
        showInt(i, 3);                  // show current time (progress)
        (void) Cconws(" s");            // show time unit

        if(i >= 10 && (i % 5) == 0) {   // every 5 seconds, check if device is alive
            BYTE res = setResolution(); // issue a simple command
    
            if(res) {                   // if succeeded, we quit fake progress
                break;
            }
        } else {
            sleep(1);                   // wait 1 second
        }

        for(j=0; j<5; j++) {            // delete the displayed time, intentionally using multiple individual Cconws() instead of one longer one, as VT52 seems to break when IKBD data isn't handled (due to Franz being updated and ce_main_app not running)
            (void) Cconws("\33D");
        }
    }

    (void) Cconws("\r\n");          // move to next line
}

void showFakeProgress(void)
{
    // show title saying we're updating
    Clear_home();
    VT52_Rev_on();
    (void) Cconws(">>>   Your device is updating.  <<<\n\r");
    (void) Cconws(">>> Do NOT turn the device off! <<<\n\r\n\r");
    VT52_Rev_off();

    (void) Cconws("The update might take up to 5 minutes\r\n");
    (void) Cconws("if installing something bigger.\r\n\r\n");

    hdIf.maxRetriesCount = 0;                               // don't retry - we're still might be updating

    showFakeProgressOfItem("Updating: ", 5*60);

    // we're done, try to reconnect.
    (void) Cconws("\r\nIf everything went well,\r\nwill connect back soon.\r\n");

    int i;
    for(i=0; i<15; i++) {
        BYTE res = showHomeScreenSimple();                  // try to reconnect and show home screen
        
        if(res == TRUE) {                                   // if reconnect succeeded, quit
            break;
        }
        
        BYTE key = getKeyIfPossible();
        
        if(key == KEY_F10 || key == 'q' || key == 'Q') {    // user requested quit by key press? quit
            break;
        }
        
        (void) Cconws(".");                                 // if reconnect failed, show dot and wait again
        sleep(1);
    }
    
    hdIf.maxRetriesCount = 1;                               // enable retry, but just once
}

BYTE getKeyIfPossible(void)
{
    DWORD scancode, special;
    BYTE key, vkey, res;

    res = Cconis();                             // see if there's something waiting from keyboard 

    if(res == 0) {                              // nothing waiting from keyboard?
        return 0;
    }
    
    scancode = Cnecin();                        // get char form keyboard, no echo on screen 
    special  = Kbshift(-1);

    vkey    = (scancode>>16)    & 0xff;
    key     =  scancode         & 0xff;

    key     = atariKeysToSingleByte(vkey, key, special & 0x03, special & 0x04); // transform BYTE pair into single BYTE
    return key;
}
//--------------------------------------------------
// global variables, later used for calling hdIfCmdAsSuper
BYTE __readNotWrite, __cmdLength;
WORD __sectorCount;
BYTE *__cmd, *__buffer;

void hdIfCmdAsSuper(void)
{
    // this should be called through Supexec()
    (*hdIf.cmd)(__readNotWrite, __cmd, __cmdLength, __buffer, __sectorCount);
}

void hdIfCmdAsUser(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    // store params to global vars
    __readNotWrite  = readNotWrite;
    __cmd           = cmd;
    __cmdLength     = cmdLength;
    __buffer        = buffer;
    __sectorCount   = sectorCount;    
    
    // call the function which does the real work, and uses those global vars
    Supexec(hdIfCmdAsSuper);
}

