#ifndef _CONFIGSTREAM_H_
#define _CONFIGSTREAM_H_

#include <stdio.h>

#include "configcomponent.h"
#include "stupidvector.h"
#include "../settingsreloadproxy.h"
#include "../version.h"

class AcsiDataTrans;

enum CS_ACTION { CS_CREATE_ACSI = 1,    CS_CREATE_TRANSLATED,   CS_CREATE_SHARED,
                 CS_CREATE_FLOPPY_CONF, 
                 CS_CREATE_NETWORK,     CS_CREATE_UPDATE,
                 CS_SAVE_ACSI,          CS_SAVE_TRANSLATED,     CS_SAVE_NETWORK,
                 CS_HIDE_MSG_SCREEN,    CS_GO_HOME,
                 CS_UPDATE_CHECK,       CS_UPDATE_UPDATE,       CS_SHARED_TEST,
                 CS_SHARED_SAVE,
                 CS_FLOPPY_IMAGE_SAVE,  CS_FLOPPY_CONFIG_SAVE
                };


enum COMPIDS {  COMPID_TRAN_FIRST = 1,      COMPID_TRAN_SHARED,         COMPID_TRAN_CONFDRIVE,
                COMPID_BTN_SAVE,            COMPID_BTN_CANCEL,
                COMPID_NET_IP,              COMPID_NET_MASK,            COMPID_NET_GATEWAY,
                COMPID_NET_DNS,             COMPID_NET_DHCP,            
				
				COMPID_WIFI_IP,             COMPID_WIFI_MASK,           COMPID_WIFI_GATEWAY,
				COMPID_WIFI_DHCP,			COMPID_WIFI_SSID,			COMPID_WIFI_PSK,
				
				COMPID_UPDATE_COSMOSEX,
                COMPID_UPDATE_FRANZ,        COMPID_UPDATE_HANZ,         COMPID_UPDATE_XILINX,
                COMPID_UPDATE_BTN_CHECK,    COMPID_SHARED_BTN_TEST,     
				
				COMPID_SHARED_IP,           COMPID_SHARED_PATH, 		COMPID_SHARED_ENABLED, 
				COMPID_SHARED_NFS_NOT_SAMBA,    COMPID_USERNAME,        COMPID_PASSWORD,

                COMPID_FLOPCONF_ENABLED,    COMPID_FLOPCONF_ID,         COMPID_FLOPCONF_WRPROT,

                COMPID_DL1,                 COMPID_DL2,                 COMPID_DL3,                 COMPID_DL4
            };

#define ST_RESOLUTION_LOW       0
#define ST_RESOLUTION_MID       1
#define ST_RESOLUTION_HIGH      2

class ConfigStream
{
public:
    ConfigStream();
    ~ConfigStream();

    // functions which are called from the main loop
    void processCommand(BYTE *cmd);
    void setAcsiDataTrans(AcsiDataTrans *dt);
    void setSettingsReloadProxy(SettingsReloadProxy *rp);

    void fillUpdateWithCurrentVersions(void);
    void fillUpdateDownloadWithProgress(void);
    void showUpdateDownloadFail(void);
    bool isUpdateDownloadPageShown(void);
    void showUpdateError(void);

    // functions which are called from various components
    int  checkboxGroup_getCheckedId(int groupId);
    void checkboxGroup_setCheckedId(int groupId, int checkedId);

    void showMessageScreen(char *msgTitle, char *msgTxt);
    void hideMessageScreen(void);

    void createScreen_homeScreen(void);
    void createScreen_acsiConfig(void);
    void createScreen_translated(void);
    void createScreen_network(void);
    void createScreen_update(void);
    void createScreen_shared(void);
    void createScreen_floppy_config(void);

    ConfigComponent *findComponentById(int compId);
    bool getTextByComponentId(int componentId, std::string &text);
    void setTextByComponentId(int componentId, std::string &text);
    bool getBoolByComponentId(int componentId, bool &val);
    void setBoolByComponentId(int componentId, bool &val);
    void focusByComponentId(int componentId);
    bool focusNextCheckboxGroup(BYTE key, int groupid, int chbid);

    void onCheckboxGroupEnter(int groupId, int checkboxId);

    void enterKeyHandlerLater(int event);
    
private:
    StupidVector screen;
    StupidVector message;

    int stScreenWidth;
    int gotoOffset;

    int enterKeyEventLater;
    
    AcsiDataTrans       *dataTrans;
    SettingsReloadProxy *reloadProxy;

    void onKeyDown(BYTE key);
    int  getStream(bool homeScreen, BYTE *bfr, int maxLen);

    bool showingHomeScreen;
    bool showingMessage;
    bool screenChanged;

    void enterKeyHandler(int event);
    
    void destroyCurrentScreen(void);
    void setFocusToFirstFocusable(void);

    void screen_addHeaderAndFooter(StupidVector &scr, char *screenName);
    void destroyScreen(StupidVector &scr);

    void onAcsiConfig_save(void);
    void onTranslated_save(void);
    void onNetwork_save(void);

    void onUpdateCheck(void);
    void onUpdateUpdate(void);
    void datesToStrings(Version &v1, Version &v2, std::string &str);
    void createScreen_update_download(void);
    void getProgressLine(int index, std::string &lines, std::string &line);

    void onSharedTest(void);
    void onSharedSave(void);

    void onFloppyConfigSave(void);

    bool verifyAndFixIPaddress(std::string &in, std::string &out, bool emptyIsOk);

    void replaceNewLineWithGoto(std::string &line, int startX, int startY);
};

#endif
