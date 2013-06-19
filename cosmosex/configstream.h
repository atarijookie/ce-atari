#include <stdio.h>
#include <vector>

#include "configcomponent.h"

class ConfigStream
{
public:
	ConfigStream();
	~ConfigStream();
	
	void onKeyDown(char vkey, char key);
	void getStream(bool homeScreen, char *bfr, int maxLen);
	
	
private:
	std::vector<ConfigComponent *> screen;

	bool showingHomeScreen;
	bool screenChanged;
	
	void createScreen_homeScreen(void);
	void destroyCurrentScreen(void);
};