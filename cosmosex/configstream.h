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
	
private:
	ConfigStream();

	std::vector<ConfigComponent *> screen;

	bool showingHomeScreen;
	bool screenChanged;
	
	void createScreen_homeScreen(void);
	void destroyCurrentScreen(void);
	
};