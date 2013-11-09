#ifndef _CONFIGSTREAM_H_
#define _CONFIGSTREAM_H_

#include <stdio.h>
#include <vector>

#include "configcomponent.h"

class AcsiDataTrans;

enum CS_ACTION { CS_CREATE_ACSI = 1,    CS_CREATE_TRANSLATED,   CS_CREATE_SHARED,
                 CS_CREATE_FLOPPY,      CS_CREATE_NETWORK,      CS_CREATE_UPDATE,
                 CS_SAVE_ACSI,          CS_SAVE_TRANSLATED,     CS_SAVE_NETWORK,
                 CS_HIDE_MSG_SCREEN,    CS_GO_HOME
                };

#define COMPID_TRAN_FIRST           1
#define COMPID_TRAN_SHARED          2
#define COMPID_TRAN_CONFDRIVE       3

#define COMPID_BTN_SAVE             0xfff0
#define COMPID_BTN_CANCEL           0xfff1

#define COMPID_NET_DHCP             4
#define COMPID_NET_IP               5
#define COMPID_NET_MASK             6
#define COMPID_NET_GATEWAY          7
#define COMPID_NET_DNS              8


class ConfigStream
{
public:
    ConfigStream();
    ~ConfigStream();

    // functions which are called from the main loop
    void processCommand(BYTE *cmd);
    void setAcsiDataTrans(AcsiDataTrans *dt);

    // functions which are called from various components
    int  checkboxGroup_getCheckedId(int groupId);
    void checkboxGroup_setCheckedId(int groupId, int checkedId);

    void showMessageScreen(char *msgTitle, char *msgTxt);
    void hideMessageScreen(void);

    void createScreen_homeScreen(void);
    void createScreen_acsiConfig(void);
    void createScreen_translated(void);
    void createScreen_network(void);

    ConfigComponent *findComponentById(int compId);
    bool getTextByComponentId(int componentId, std::string &text);
    void setTextByComponentId(int componentId, std::string &text);
    bool getBoolByComponentId(int componentId, bool &val);
    void setBoolByComponentId(int componentId, bool &val);
    void focusByComponentId(int componentId);
    bool focusNextCheckboxGroup(BYTE key, int groupid, int chbid);

    void enterKeyHandler(int event);
    void onCheckboxGroupEnter(int groupId, int checkboxId);

private:
    std::vector<ConfigComponent *> screen;
    std::vector<ConfigComponent *> message;

    AcsiDataTrans *dataTrans;

    void onKeyDown(BYTE key);
    int  getStream(bool homeScreen, BYTE *bfr, int maxLen);

    bool showingHomeScreen;
    bool showingMessage;
    bool screenChanged;

    void destroyCurrentScreen(void);
    void setFocusToFirstFocusable(void);

    void screen_addHeaderAndFooter(std::vector<ConfigComponent *> &scr, char *screenName);
    void destroyScreen(std::vector<ConfigComponent *> &scr);

    void onAcsiConfig_save(void);
    void onTranslated_save(void);
    void onNetwork_save(void);

    bool verifyAndFixIPaddress(std::string &in, std::string &out, bool emptyIsOk);
};

#endif
