#include "settings.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#define SETTINGS_PATH		"settings"
#include "debug.h"

Settings::Settings(void)
{
	int res = mkdir(SETTINGS_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);		// mod: 0x775
	
	if(res == 0) {					// dir created
		Debug::out("Settings: directory %s was created.", SETTINGS_PATH);
	} else {						// dir not created
		if(errno != EEXIST) {		// and it's not because it already exists...
			Debug::out("Settings: failed to create settings directory - %s", strerror(errno));
		}
	}
}

bool Settings::getBool(char *key, bool defValue)
{
	FILE *file = open(key, true);
	if(!file) {											// failed to open settings?
//		Debug::out("Settings::getBool -- returning default value for %s", key);
		return defValue;
	}

	int val, res;
	res = fscanf(file, "%d", &val);						// try to read the value
	fclose(file);
	
	if(res != 1) {										// failed to read value?
//		Debug::out("Settings::getBool -- returning default value for %s", key);
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
		Debug::out("Settings::setBool -- could not write key %s", key);
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
//		Debug::out("Settings::getInt -- returning default value for %s", key);
		return defValue;
	}

	int val, res;
	res = fscanf(file, "%d", &val);						// try to read the value
	fclose(file);
	
	if(res != 1) {										// failed to read value?
//		Debug::out("Settings::getInt -- returning default value for %s", key);
		return defValue;
	}
	
	return val;											// return
}

void Settings::setInt(char *key, int value)
{
	FILE *file = open(key, false);
	if(!file) {											// failed to open settings?
		Debug::out("Settings::setInt -- could not write key %s", key);
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
//		Debug::out("Settings::getString -- returning default value for %s", key);
		strcpy(buffer, defValue);
		return buffer;
	}

	char *res;
	res = fgets(buffer, 256, file);						// try to read the value
	fclose(file);
	
	if(res == NULL) {									// failed to read value?
//		Debug::out("Settings::getString -- returning default value for %s", key);
		strcpy(buffer, defValue);
		return buffer;
	}
	
	return buffer;											// return
}

void Settings::setString(char *key, char *value)
{
	FILE *file = open(key, false);
	if(!file) {											// failed to open settings?
		Debug::out("Settings::setString -- could not write key %s", key);
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
//        Debug::out("Settings::getChar -- returning default value for %s", key);
        return defValue;
    }

    int res;
    char val;
    res = fscanf(file, "%c", &val);						// try to read the value
    fclose(file);

    if(res != 1) {										// failed to read value?
//        Debug::out("Settings::getChar -- returning default value for %s", key);
        return defValue;
    }

    return val;											// return
}

void Settings::setChar(char *key, char value)
{
    FILE *file = open(key, false);
    if(!file) {											// failed to open settings?
        Debug::out("Settings::setChar -- could not write key %s", key);
        return;
    }

    fputc(value, file);
    fclose(file);
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

