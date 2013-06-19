#include "settings.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#define SETTINGS_PATH		"settings"

Settings::Settings(void)
{
	printf("Creating dir: %s\n", SETTINGS_PATH);
	
	char bfr[1024];
	strcpy(bfr, "mkdir ");
	strcat(bfr, SETTINGS_PATH);
	system(bfr);
}

bool Settings::getBool(char *key, bool defValue)
{
	FILE *file = open(key, true);
	if(!file) {											// failed to open settings?
		printf("Settings::getBool -- returning default value for %s\n", key);
		return defValue;
	}

	int val, res;
	res = fscanf(file, "%d", &val);						// try to read the value
	fclose(file);
	
	if(res != 1) {										// failed to read value?
		printf("Settings::getBool -- returning default value for %s\n", key);
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
		printf("Settings::setBool -- could not write key %s\n", key);
		return;
	}

	if(value) {					// if true, write 1
		fputs("1\n", file);
	} else {					// if false, write 0
		fputs("0\n", file);
	}

	fclose(file);
}
	
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

