#ifndef _CONFIGSTREAM_H_
#define _CONFIGSTREAM_H_

#include <stdio.h>
#include <vector>

#include "configcomponent.h"

void onCheckboxGroupEnter(int groupId, int checkboxId);

class ConfigStream
{
public:
	static ConfigStream& instance(void) {
		static ConfigStream configStream;
		return configStream;
	}
	
	~ConfigStream();
	
	void onKeyDown(char vkey, char key);
	void getStream(bool homeScreen, char *bfr, int maxLen);
	
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

	bool showingHomeScreen;
	bool showingMessage;
	bool screenChanged;
	
	void destroyCurrentScreen(void);
	void setFocusToFirstFocusable(void);	
	
	void screen_addHeaderAndFooter(std::vector<ConfigComponent *> &scr, char *screenName);
	void destroyScreen(std::vector<ConfigComponent *> &scr);
};

#endif
