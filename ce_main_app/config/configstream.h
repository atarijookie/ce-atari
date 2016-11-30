// vim: expandtab shiftwidth=4 tabstop=4
#ifndef _CONFIGSTREAM_H_
#define _CONFIGSTREAM_H_

#include <stdio.h>

#include "configcomponent.h"
#include "../stupidvector.h"
#include "../settingsreloadproxy.h"
#include "../version.h"

#define CONFIG_TEXT_FILE        "/tmp/ce_config.txt"

#define CONFIGSTREAM_ON_ATARI           0
#define CONFIGSTREAM_IN_LINUX_CONSOLE   1
#define CONFIGSTREAM_THROUGH_WEB        2

class TranslatedDisk;
class AcsiDataTrans;

enum CS_ACTION { CS_CREATE_ACSI = 1,    CS_CREATE_TRANSLATED,   CS_CREATE_SHARED,
                 CS_CREATE_FLOPPY_CONF, CS_CREATE_IKBD,         CS_CREATE_HDDIMAGE,
                 CS_CREATE_NETWORK,     CS_CREATE_UPDATE,       CS_CREATE_OTHER,
                 CS_SAVE_ACSI,          CS_SAVE_TRANSLATED,     CS_SAVE_NETWORK,
                 CS_HDDIMAGE_SAVE,
                 CS_HIDE_MSG_SCREEN,    CS_GO_HOME,
                 CS_UPDATE_CHECK,       CS_UPDATE_CHECK_USB,    CS_UPDATE_UPDATE,
                 CS_SHARED_TEST,        CS_SHARED_SAVE,
                 CS_FLOPPY_IMAGE_SAVE,  CS_FLOPPY_CONFIG_SAVE,
                 CS_OTHER_SAVE,         CS_RESET_SETTINGS,      CS_SEND_SETTINGS,
                 CS_IKBD_SAVE
                };


enum COMPIDS {  COMPID_TRAN_FIRST = 1,      COMPID_TRAN_SHARED,         COMPID_TRAN_CONFDRIVE,
                COMPID_MOUNT_RAW_NOT_TRANS, 
                COMPID_BTN_SAVE,            COMPID_BTN_CANCEL,
                COMPID_USE_ZIP_DIR_NOT_FILE,

                COMPID_HOSTNAME,
                COMPID_NET_IP,              COMPID_NET_MASK,            COMPID_NET_GATEWAY,
                COMPID_NET_DNS,             COMPID_NET_DHCP,            
				
				COMPID_WIFI_IP,             COMPID_WIFI_MASK,           COMPID_WIFI_GATEWAY,
				COMPID_WIFI_DHCP,			COMPID_WIFI_SSID,			COMPID_WIFI_PSK,
                
				COMPID_UPDATE_COSMOSEX,     COMPID_UPDATE_LOCATION,
                COMPID_UPDATE_FRANZ,        COMPID_UPDATE_HANZ,             COMPID_UPDATE_XILINX,
                COMPID_UPDATE_BTN_CHECK,    COMPID_UPDATE_BTN_CHECK_USB,    COMPID_SHARED_BTN_TEST,     
				
                COMPID_HDDIMAGE_PATH,
				COMPID_SHARED_IP,           COMPID_SHARED_PATH, 		COMPID_SHARED_ENABLED, 
				COMPID_SHARED_NFS_NOT_SAMBA,    COMPID_USERNAME,        COMPID_PASSWORD,

                COMPID_FLOPCONF_ENABLED,    COMPID_FLOPCONF_ID,         COMPID_FLOPCONF_WRPROT,

                COMPID_DL_TITLE,            COMPID_DL1,
                COMPID_TIMESYNC_ENABLE,     COMPID_TIMESYNC_NTP_SERVER, COMPID_TIMESYNC_UTC_OFFSET,
                COMPID_SCREENCAST_FRAMESKIP,    COMPID_SCREEN_RESOLUTION,
                COMPID_JOY0_FIRST,          COMPID_MOUSEWHEEL_ENABLED,  COMPID_KEYB_JOY0, COMPID_KEYB_JOY1,
                
                COMPID_KEYBJOY0_BUTTON, COMPID_KEYBJOY0_LEFT, COMPID_KEYBJOY0_DOWN, COMPID_KEYBJOY0_RIGHT, COMPID_KEYBJOY0_UP,
                COMPID_KEYBJOY1_BUTTON, COMPID_KEYBJOY1_LEFT, COMPID_KEYBJOY1_DOWN, COMPID_KEYBJOY1_RIGHT, COMPID_KEYBJOY1_UP
            };

#define ST_RESOLUTION_LOW       0
#define ST_RESOLUTION_MID       1
#define ST_RESOLUTION_HIGH      2

#define READ_BUFFER_SIZE    (5 * 1024)

class ConfigStream
{
public:
    ConfigStream(int whereItWillBeShown);
    ~ConfigStream();

    // functions which are called from the main loop
    void processCommand(BYTE *cmd, int writeToFd=-1);
    void setAcsiDataTrans(AcsiDataTrans *dt);
    void setSettingsReloadProxy(SettingsReloadProxy *rp);

    void fillUpdateWithCurrentVersions(void);
    void fillUpdateDownloadWithProgress(void);
    void fillUpdateDownloadWithFinish(void);
    void showUpdateDownloadFail(void);
    bool isUpdateDownloadPageShown(void);
    void showUpdateError(void);

    // functions which are called from various components
    int  checkboxGroup_getCheckedId(int groupId);
    void checkboxGroup_setCheckedId(int groupId, int checkedId);

    void showMessageScreen(const char *msgTitle, const char *msgTxt);
    void hideMessageScreen(void);

    void createConfigDump(void);
    
    void createScreen_homeScreen(void);
    void createScreen_acsiConfig(void);
    void createScreen_translated(void);
    void createScreen_network(void);
    void createScreen_update(void);
    void createScreen_hddimage(void);
    void createScreen_shared(void);
    void createScreen_floppy_config(void);
    void createScreen_other(void);
    void createScreen_ikbd(void);

    ConfigComponent *findComponentById(int compId);
    bool getTextByComponentId(int componentId, std::string &text);
    void setTextByComponentId(int componentId, std::string &text);
    bool getBoolByComponentId(int componentId, bool &val);
    void setBoolByComponentId(int componentId, bool &val);
    void setIntByComponentId(int componentId, int value);
    bool getIntByComponentId(int componentId, int &value);
    void setFloatByComponentId(int componentId, float value);
    bool getFloatByComponentId(int componentId, float &value);

    void focusByComponentId(int componentId);
    bool focusNextCheckboxGroup(BYTE key, int groupid, int chbid);

    void onCheckboxGroupEnter(int groupId, int checkboxId);

    void enterKeyHandlerLater(int event);

    void setTranslatedDisk(TranslatedDisk *td) { this->translated = td; };
private:
    // properties
    int shownOn;

    StupidVector screen;
    StupidVector message;

    int stScreenWidth;
    int gotoOffset;

    bool updateFromWebNotUsb;
    
    int enterKeyEventLater;
    
    AcsiDataTrans       *dataTrans;
    SettingsReloadProxy *reloadProxy;
    TranslatedDisk      *translated;

    // private methods
    void onKeyDown(BYTE key);
    int  getStream(bool homeScreen, BYTE *bfr, int maxLen);

    bool showingHomeScreen;
    bool showingMessage;
    bool screenChanged;

    void enterKeyHandler(int event);
    
    void destroyCurrentScreen(void);
    void setFocusToFirstFocusable(void);

    void screen_addHeaderAndFooter(StupidVector &scr, const char *screenName);
    void destroyScreen(StupidVector &scr);

    void onAcsiConfig_save(void);
    void onTranslated_save(void);
    void onNetwork_save(void);

    void onUpdateCheck(void);
    void onUpdateCheckUsb(void);
    void onUpdateUpdate(void);
    BYTE isUpdateScreen(void);
    void datesToStrings(Version &v1, Version &v2, std::string &str);
    void createScreen_update_download(void);
    void getProgressLine(int index, std::string &lines, std::string &line);

    void onHddImageSave(void);

    void onSharedTest(void);
    void onSharedSave(void);

    void onFloppyConfigSave(void);

    void onOtherSave(void);
    void onIkbdSave(void);
    void onResetSettings(void);
    void onSendSettings(void);
    
    bool verifyAndFixIPaddress(std::string &in, std::string &out, bool emptyIsOk);

    void replaceNewLineWithGoto(std::string &line, int startX, int startY);
    
    void translateVT52rawConsole(const BYTE *vt52stream, int vt52cnt, char *rawConsole, int rawConsoleSize);
    void dumpScreenToFile(FILE *f);
    //-------------
    // remote console stuff
    void linuxConsole_KeyDown(BYTE atariKey);
    int  linuxConsole_getStream(BYTE *bfr, int maxLen);
    
    int  filterVT100(char *bfr, int cnt);
    void atariKeyToConsoleKey(BYTE atariKey, char *bfr, int &cnt);
    // Set values
    void onSetCfgValue(void);
};

#endif
