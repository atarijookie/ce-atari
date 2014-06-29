#include "settings.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#define SETTINGS_PATH		"/ce/settings"

#include "global.h"
#include "debug.h"

extern bool g_test;                                     // if set to true, set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image

Settings::Settings(void)
{
	int res = mkdir(SETTINGS_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);		// mod: 0x775
	
	if(res == 0) {					// dir created
		Debug::out(LOG_INFO, "Settings: directory %s was created.", SETTINGS_PATH);
		
		storeDefaultValues();
	} else {						// dir not created
		if(errno != EEXIST) {		// and it's not because it already exists...
			Debug::out(LOG_ERROR, "Settings: failed to create settings directory - %s", strerror(errno));
		}
	}
}

void Settings::storeDefaultValues(void)
{
    char key[32];
    for(int id=0; id<8; id++) {							// read the list of device types from settings
        sprintf(key, "ACSI_DEVTYPE_%d", id);			// create settings KEY, e.g. ACSI_DEVTYPE_0
		
		if(id == 0) {									// ACSI id 0 enaled by defaul
			setInt(key, DEVTYPE_TRANSLATED);
		} else {										// other ACSI id's disabled
			setInt(key, DEVTYPE_OFF);
		}
	}
	
	setChar((char *) "DRIVELETTER_FIRST",      'C');
    setChar((char *) "DRIVELETTER_SHARED",     'P');
    setChar((char *) "DRIVELETTER_CONFDRIVE",  'O');
	
	setBool((char *) "SHARED_ENABLED",			false);
	setBool((char *) "SHARED_NFS_NOT_SAMBA",	false);
}

bool Settings::getBool(char *key, bool defValue)
{
	FILE *file = open(key, true);
	if(!file) {											// failed to open settings?
//		Debug::out(LOG_DEBUG, "Settings::getBool -- returning default value for %s", key);
		return defValue;
	}

	int val, res;
	res = fscanf(file, "%d", &val);						// try to read the value
	fclose(file);
	
	if(res != 1) {										// failed to read value?
//		Debug::out(LOG_DEBUG, "Settings::getBool -- returning default value for %s", key);
		return defValue;
	}
	
	bool ret = false;									// convert int to bool
	if(val == 1) {
		ret = true;
	}
	
	return ret;											// return
}

void Settings::setBool(char *key, bool value)
{
	FILE *file = open(key, false);
	if(!file) {											// failed to open settings?
		Debug::out(LOG_ERROR, "Settings::setBool -- could not write key %s", key);
		return;
	}

	if(value) {					// if true, write 1
		fputs("1\n", file);
	} else {					// if false, write 0
		fputs("0\n", file);
	}

	fclose(file);
}
	
//-------------------------	
int Settings::getInt(char *key, int defValue)
{
	FILE *file = open(key, true);
	if(!file) {											// failed to open settings?
//		Debug::out(LOG_DEBUG, "Settings::getInt -- returning default value for %s", key);
		return defValue;
	}

	int val, res;
	res = fscanf(file, "%d", &val);						// try to read the value
	fclose(file);
	
	if(res != 1) {										// failed to read value?
//		Debug::out(LOG_DEBUG, "Settings::getInt -- returning default value for %s", key);
		return defValue;
	}
	
	return val;											// return
}

void Settings::setInt(char *key, int value)
{
	FILE *file = open(key, false);
	if(!file) {											// failed to open settings?
		Debug::out(LOG_ERROR, "Settings::setInt -- could not write key %s", key);
		return;
	}

	fprintf(file, "%d\n", value);
	fclose(file);
}
//-------------------------	
char *Settings::getString(char *key, char *defValue)
{
	static char buffer[256];
	memset(buffer, 0, 256);

	FILE *file = open(key, true);
	if(!file) {											// failed to open settings?
//		Debug::out(LOG_DEBUG, "Settings::getString -- returning default value for %s", key);
		strcpy(buffer, defValue);
		return buffer;
	}

	char *res;
	res = fgets(buffer, 256, file);						// try to read the value
	fclose(file);
	
	if(res == NULL) {									// failed to read value?
//		Debug::out(LOG_DEBUG, "Settings::getString -- returning default value for %s", key);
		strcpy(buffer, defValue);
		return buffer;
	}
	
	return buffer;											// return
}

void Settings::setString(char *key, char *value)
{
	FILE *file = open(key, false);
	if(!file) {											// failed to open settings?
		Debug::out(LOG_ERROR, "Settings::setString -- could not write key %s", key);
		return;
	}

	fputs(value, file);
	fclose(file);
}	
//-------------------------
char Settings::getChar(char *key, char defValue)
{
    FILE *file = open(key, true);
    if(!file) {											// failed to open settings?
//        Debug::out(LOG_DEBUG, "Settings::getChar -- returning default value for %s", key);
        return defValue;
    }

    int res;
    char val;
    res = fscanf(file, "%c", &val);						// try to read the value
    fclose(file);

    if(res != 1) {										// failed to read value?
//        Debug::out(LOG_DEBUG, "Settings::getChar -- returning default value for %s", key);
        return defValue;
    }

    return val;											// return
}

void Settings::setChar(char *key, char value)
{
    FILE *file = open(key, false);
    if(!file) {											// failed to open settings?
        Debug::out(LOG_ERROR, "Settings::setChar -- could not write key %s", key);
        return;
    }

    fputc(value, file);
    fclose(file);
}
//-------------------------
void Settings::loadAcsiIDs(AcsiIDinfo *aii)
{
    aii->enabledIDbits = 0;									// no bits / IDs enabled yet

	aii->gotDevTypeRaw			= false;					// no raw and translated types found yet
	aii->gotDevTypeTranslated	= false;
	aii->gotDevTypeSd			= false;
	
	aii->sdCardAcsiId = 0xff;								// at start mark that we don't have SD card ID yet
	
    char key[32];
    for(int id=0; id<8; id++) {							// read the list of device types from settings
        sprintf(key, "ACSI_DEVTYPE_%d", id);			// create settings KEY, e.g. ACSI_DEVTYPE_0
        
		int devType = getInt(key, DEVTYPE_OFF);

        if(devType < 0) {
            devType = DEVTYPE_OFF;
        }

        //-------------------------
        // if we're in testing mode
        if(g_test) {
            switch(id) {
            case 0:     devType = DEVTYPE_TRANSLATED;   break;
            case 1:     devType = DEVTYPE_SD;           break;
            default:    devType = DEVTYPE_OFF;          break;
            }
        }
        //-------------------------
        
        aii->acsiIDdevType[id] = devType;

        if(devType == DEVTYPE_SD) {                     // if on this ACSI ID we should have the native SD card, store this ID
            aii->sdCardAcsiId = id;
			aii->gotDevTypeSd = true;
        }

        if(devType != DEVTYPE_OFF) {                    // if ON
            aii->enabledIDbits |= (1 << id);            // set the bit to 1
        }
		
		if(devType == DEVTYPE_RAW) {					// found at least one RAW device?
			aii->gotDevTypeRaw = true;
		}
		
		if(devType == DEVTYPE_TRANSLATED) {				// found at least one TRANSLATED device?
			aii->gotDevTypeTranslated = true;
		}
    }

	// no ACSI ID was enabled? enable ACSI ID 0
	if(!aii->gotDevTypeRaw && !aii->gotDevTypeTranslated && !aii->gotDevTypeSd) {
		Debug::out(LOG_INFO, "Settings::loadAcsiIDs -- no ACSI ID was enabled, so enabling ACSI ID 0");
			
		aii->acsiIDdevType[0]	= DEVTYPE_TRANSLATED;
		aii->enabledIDbits	= 1;
		
		aii->gotDevTypeTranslated = true;
	}
}
//-------------------------

FILE *Settings::open(char *key, bool readNotWrite)
{
	char path[1024];
	
	strcpy(path, SETTINGS_PATH);
	strcat(path, "/");
	strcat(path, key);

	FILE *file;
	
	if(readNotWrite) {
		file = fopen(path, "r");				// try to open file for reading
	} else {
		file = fopen(path, "w");				// try to open file for writing
	}
	
	return file;
}

