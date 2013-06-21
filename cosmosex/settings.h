#ifndef _CONFIGSCREEN_MAIN_H_
#define _SETTINGS_H_

#include <stdio.h>

class Settings 
{
public:
	Settings(void);

	bool getBool(char *key, bool defValue);
	void setBool(char *key, bool value);
	
private:

	FILE *open(char *key, bool readNotWrite);
};

#endif
