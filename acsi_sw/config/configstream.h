#ifndef _CONFIGSTREAM_H_
#define _CONFIGSTREAM_H_

#include <stdio.h>
#include <vector>

#include "configcomponent.h"

class AcsiDataTrans;

void onCheckboxGroupEnter(int groupId, int checkboxId);

class ConfigStream
{
public:
    static ConfigStream& instance(void) {
        static ConfigStream configStream;
        return configStream;
    }

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

private:
    ConfigStream();

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
};

#endif
