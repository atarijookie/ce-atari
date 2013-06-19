#include <stdio.h>
#include <vector>

#include "configcomponent.h"

class ConfigStream
{
public:
	ConfigStream(void);
	
	void onKeyDown(char vkey, char key);

	void goToHomeScreen(void);
	void getStream(char *bfr, int maxLen);
	
	
private:
	std::vector<ConfigComponent *> screen;

};