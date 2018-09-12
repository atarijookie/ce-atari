//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <mint/linea.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "global.h"
#include "../ce_hdd_if/stdlib.h"
#include "../ce_hdd_if/hdd_if.h"
#include "keys.h"
#include "vt52.h"

//--------------------------------------------------
void hdIfCmdAsUser(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

void showHomeScreen(void);
void sendKeyDown(BYTE key, BYTE keyDownCommand);
void refreshScreen(void);
BYTE setResolution(void);
void showConnectionErrorMessage(void);
void showMoreStreamIfNeeded(void);

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
//--------------------------------------------------

#define BUFFER_SIZE         (4*512)
BYTE myBuffer[BUFFER_SIZE];
BYTE *pBuffer;

BYTE prevCommandFailed;

//--------------------------------------------------
int main(void)
{
    BYTE key;
    DWORD toEven;
    BYTE keyDownCommand = CFG_CMD_KEYDOWN;
    DWORD lastUpdateCheckTime = 0;
    BYTE devTypeFound;

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

    Clear_home();
    deviceID = findDevice(IF_ANY, (DEV_CE | DEV_CS));

    if(deviceID == DEVICE_NOT_FOUND) {
        return 0;
    }

    //------------------
    // if the device is CosmoSolo, go this way
    devTypeFound = getDevTypeFound();

    if(devTypeFound == DEV_CS) {
        cosmoSoloConfig();
        return 0;
    }
    
    // ----------------- 
    // if the device is CosmosEx, do the remote console config
    hdIf.maxRetriesCount = 1;                                       // retry only once
    
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

        if(key == KEY_F8) {                                         // should switch between config and linux console?
            Clear_home();                                           // clear the screen
        
            if(keyDownCommand == CFG_CMD_KEYDOWN) {                 // currently showing normal config?
                Cursor_on();                                        // turn on cursor            
                keyDownCommand = CFG_CMD_LINUXCONSOLE_GETSTREAM;    // switch to linux console
                sendKeyDown(KEY_ENTER, keyDownCommand);             // send enter to show the command line
            } else {                                                // showing linux console? 
                keyDownCommand = CFG_CMD_KEYDOWN;                   // switch to normal config
                refreshScreen();                                    // refresh the screen
            }
            
            lastShowStreamTime = getTicksAsUser();                  // we just shown the stream, no need for refresh
            continue;
        }
        
        if(key == KEY_F10) {                                        // should quit? 
            break;
        }
        
        if(key == KEY_F5 && keyDownCommand == CFG_CMD_KEYDOWN) {    // should refresh? and are we on the config part, not the linux console part? 
            refreshScreen();
            lastShowStreamTime = getTicksAsUser();                  // we just shown the stream, no need for refresh
            continue;
        }
        
        sendKeyDown(key, keyDownCommand);                           // send this key to device
        lastShowStreamTime = getTicksAsUser();                      // we just shown the stream, no need for refresh
    }
    
    return 0;
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
    
    if(keyDownCommand == CFG_CMD_LINUXCONSOLE_GETSTREAM) {  // if we're on the linux console stream, possibly show more data
        showMoreStreamIfNeeded();                           // if there's a marker about more data, fetch it
    }
}
//--------------------------------------------------
void showMoreStreamIfNeeded(void)
{
    BYTE cmd[] = {0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_LINUXCONSOLE_GETSTREAM, 0};
    
    cmd[0] = (deviceID << 5);                           // cmd[0] = ACSI_id + TEST UNIT READY (0)   
    cmd[5] = 0;                                         // no key pressed 
  
    while(1) {
        if(pBuffer[ (3 * 512) - 1 ] == LINUXCONSOLE_NO_MORE_DATA) {     // no more data? quit
            break;
        }
    
        memset(pBuffer, 0, 3 * 512);                    // clear the buffer 
  
        hdIfCmdAsUser(1, cmd, 6, pBuffer, 3);           // issue the KEYDOWN command and show the screen stream 
    
        if(!hdIf.success || hdIf.statusByte != OK) {    // if failed, return FALSE 
            return;
        }

        (void) Cconws((char *) pBuffer);                // now display the buffer
    }
} 

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
    (void) Cconws(title);           // show what we are updating
    
    int i;
    for(i=0; i<=timeout; i++) {     // wait the whole timeout
        showInt(i, 2);              // show current time (progress)
        (void) Cconws(" s");        // show time unit
        sleep(1);                   // wait 1 second
        (void) Cconws("\33D\33D\33D\33D");
    }

    (void) Cconws("\r\n");          // move to next line
}

void showFakeProgress(void)
{
    // retrieve individual update components flags
    BYTE updatingApp    = updateComponents & UPDATECOMPONENT_APP;
    BYTE updatingXilinx = updateComponents & UPDATECOMPONENT_XILINX;
    BYTE updatingHans   = updateComponents & UPDATECOMPONENT_HANS;
    BYTE updatingFranz  = updateComponents & UPDATECOMPONENT_FRANZ;

    // show title saying we're updating
    Clear_home();
    VT52_Rev_on();
    (void) Cconws(">>>   Your device is updating.  <<<\n\r");
    (void) Cconws(">>> Do NOT turn the device off! <<<\n\r\n\r");
    VT52_Rev_off();
    
    const char *titleUpdateApp      = "Updating app    ( 5 s): ";
    const char *titleUpdateXilinx   = "Updating Xilinx (50 s): ";
    const char *titleUpdateHans     = "Updating Hans   (10 s): ";
    const char *titleUpdateFranz    = "Updating Franz  (10 s): ";
    const char *titleRestartingApp  = "Restarting app  (15 s): ";

    // first show titles, so user will know how long it will all take and what will be updated
    if(updatingApp)     { (void) Cconws(titleUpdateApp);        (void) Cconws("\r\n"); }
    if(updatingXilinx)  { (void) Cconws(titleUpdateXilinx);     (void) Cconws("\r\n"); }
    if(updatingHans)    { (void) Cconws(titleUpdateHans);       (void) Cconws("\r\n"); }
    if(updatingFranz)   { (void) Cconws(titleUpdateFranz);      (void) Cconws("\r\n"); }
                          (void) Cconws(titleRestartingApp);    
    VT52_Goto_pos(0, 3);
    
    // now possibly wait for each component to be done
    if(updatingApp) {
        showFakeProgressOfItem(titleUpdateApp, 5);
    }

    if(updatingXilinx) {
        showFakeProgressOfItem(titleUpdateXilinx, 50);
    }

    if(updatingHans) {
        showFakeProgressOfItem(titleUpdateHans, 10);
    }

    if(updatingFranz) {
        showFakeProgressOfItem(titleUpdateFranz, 10);
    }

    // everything installed, but main app needs to start and possibly update script
    showFakeProgressOfItem(titleRestartingApp, 15);
    
    // we're done, try to reconnect.
    (void) Cconws("\r\nIf everything went well,\r\nwill connect back soon.\r\n");
    
    hdIf.maxRetriesCount = 0;                               // don't retry - we're still might be updating
    
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

