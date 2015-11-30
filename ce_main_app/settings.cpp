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

extern TFlags       flags;
extern THwConfig    hwConfig;

Settings::Settings(void)
{
	int res = mkdir(SETTINGS_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);		// mod: 0x775
	
	if(res == 0) {					// dir created
		Debug::out(LOG_DEBUG, "Settings: directory %s was created.", SETTINGS_PATH);
		
		storeDefaultValues();
	} else {						// dir not created
		if(errno != EEXIST) {		// and it's not because it already exists...
			Debug::out(LOG_ERROR, "Settings: failed to create settings directory - %s", strerror(errno));
		}
	}
}

void Settings::storeDefaultValues(void)
{
    Debug::out(LOG_DEBUG, "Settings::storeDefaultValues() - storing default settings, because it seems we miss those setting...");
    
    char key[32];
    for(int id=0; id<8; id++) {							// read the list of device types from settings
        sprintf(key, "ACSI_DEVTYPE_%d", id);			// create settings KEY, e.g. ACSI_DEVTYPE_0
		
		if(id == 1) {									// ACSI id 0 enaled by defaul
			setInt(key, DEVTYPE_TRANSLATED);
		} else {										// other ACSI id's disabled
			setInt(key, DEVTYPE_OFF);
		}
	}
	
	setChar((char *) "DRIVELETTER_FIRST",      'C');
    setChar((char *) "DRIVELETTER_SHARED",     'P');
    setChar((char *) "DRIVELETTER_CONFDRIVE",  'O');

	setBool((char *) "MOUNT_RAW_NOT_TRANS",     false);
	
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
float Settings::getFloat(char *key, float defValue)
{
	FILE *file = open(key, true);
	if(!file) {											// failed to open settings?
		return defValue;
	}

	int res;
    float val;
	res = fscanf(file, "%f", &val);						// try to read the value
	fclose(file);
	
	if(res != 1) {										// failed to read value?
		return defValue;
	}
	
	return val;											// return
}

void Settings::setFloat(char *key, float value)
{
	FILE *file = open(key, false);
	if(!file) {											// failed to open settings?
		Debug::out(LOG_ERROR, "Settings::setFloat -- could not write key %s", key);
		return;
	}

	fprintf(file, "%f\n", value);
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
void Settings::loadAcsiIDs(AcsiIDinfo *aii, bool useDefaultsIfNoSettings)
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
        if(flags.test) {
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
        if(useDefaultsIfNoSettings) {                   // if should use defaults if no settings found, store those defaults and call this function again
            storeDefaultValues();
            loadAcsiIDs(aii, false);                    // ...but call this function without storing defaults next time - to avoid endless loop in some weird case
        }     
	}
}
//-------------------------
void Settings::loadFloppyConfig(FloppyConfig *fc)
{
    fc->enabled         = getBool((char *)    "FLOPPYCONF_ENABLED",           true);
    fc->id              = getInt((char *)     "FLOPPYCONF_DRIVEID",           0);
    fc->writeProtected  = getBool((char *)    "FLOPPYCONF_WRITEPROTECTED",    false);
}

void Settings::saveFloppyConfig(FloppyConfig *fc)
{
    setBool((char *)    "FLOPPYCONF_ENABLED",           fc->enabled);
    setInt((char *)     "FLOPPYCONF_DRIVEID",           fc->id);
    setBool((char *)    "FLOPPYCONF_WRITEPROTECTED",    fc->writeProtected);
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

