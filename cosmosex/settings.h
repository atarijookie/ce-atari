#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdio.h>
#include "datatypes.h"

typedef struct {
	BYTE acsiIDdevType[8];								// array of device types for each ACSI ID
	BYTE sdCardAcsiId;									// ACSI ID assigned to SD card

	BYTE enabledIDbits;									// bit map of which ACSI IDs are enabled

	bool gotDevTypeRaw;
	bool gotDevTypeTranslated;
	bool gotDevTypeSd;
} AcsiIDinfo;

typedef struct {
    bool enabled;
    int  id;
    bool writeProtected;
} FloppyConfig;

class Settings 
{
public:
	Settings(void);
	virtual ~Settings(void) { };

	bool getBool(char *key, bool defValue);
	void setBool(char *key, bool value);

	int  getInt(char *key, int defValue);
	void setInt(char *key, int value);
	
	char *getString(char *key, char *defValue);
	void  setString(char *key, char *value);

    char getChar(char *key, char defValue);
    void setChar(char *key, char value);

	void loadAcsiIDs(AcsiIDinfo *aii);

    void loadFloppyConfig(FloppyConfig *fc);
    void saveFloppyConfig(FloppyConfig *fc);
	
private:

	FILE *open(char *key, bool readNotWrite);
	
	void storeDefaultValues(void);
};

#endif
