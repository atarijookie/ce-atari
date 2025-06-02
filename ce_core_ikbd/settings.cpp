#include "settings.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#include "global.h"
#include "debug.h"
#include "utils.h"

extern TFlags       flags;

Settings::Settings(void)
{
    std::string settingsDir = Utils::dotEnvValue("SETTINGS_DIR", "./settings"); // path to settings dir
    int res = mkdir(settingsDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);      // mod: 0x775

    if(res == 0) {                  // dir created
        Debug::out(LOG_DEBUG, "Settings: directory %s was created.", settingsDir.c_str());

        storeDefaultValues();
    } else {                        // dir not created
        if(errno != EEXIST) {       // and it's not because it already exists...
            Debug::out(LOG_ERROR, "Settings: failed to create settings directory %s - %s", settingsDir.c_str(), strerror(errno));
        }
    }
}

void Settings::storeDefaultValues(void)
{
    Debug::out(LOG_DEBUG, "Settings::storeDefaultValues() - storing default settings, because it seems we miss those setting...");

}

bool Settings::getBool(const char *key, bool defValue)
{
    FILE *file = sOpen(key, true);
    if(!file) {                                         // failed to open settings?
//      Debug::out(LOG_DEBUG, "Settings::getBool -- returning default value for %s", key);
        return defValue;
    }

    int val, res;
    res = fscanf(file, "%d", &val);                     // try to read the value
    fclose(file);

    if(res != 1) {                                      // failed to read value?
//      Debug::out(LOG_DEBUG, "Settings::getBool -- returning default value for %s", key);
        return defValue;
    }

    bool ret = false;                                   // convert int to bool
    if(val == 1) {
        ret = true;
    }

    return ret;                                         // return
}

void Settings::setBool(const char *key, bool value)
{
    FILE *file = sOpen(key, false);
    if(!file) {                                         // failed to open settings?
        Debug::out(LOG_ERROR, "Settings::setBool -- could not write key %s", key);
        return;
    }

    if(value) {                 // if true, write 1
        fputs("1\n", file);
    } else {                    // if false, write 0
        fputs("0\n", file);
    }

    fclose(file);
}

//-------------------------
int Settings::getInt(const char *key, int defValue)
{
    FILE *file = sOpen(key, true);
    if(!file) {                                         // failed to open settings?
//      Debug::out(LOG_DEBUG, "Settings::getInt -- returning default value for %s", key);
        return defValue;
    }

    int val, res;
    res = fscanf(file, "%d", &val);                     // try to read the value
    fclose(file);

    if(res != 1) {                                      // failed to read value?
//      Debug::out(LOG_DEBUG, "Settings::getInt -- returning default value for %s", key);
        return defValue;
    }

    return val;                                         // return
}

void Settings::setInt(const char *key, int value)
{
    FILE *file = sOpen(key, false);
    if(!file) {                                         // failed to open settings?
        Debug::out(LOG_ERROR, "Settings::setInt -- could not write key %s", key);
        return;
    }

    fprintf(file, "%d\n", value);
    fclose(file);
}
//-------------------------
float Settings::getFloat(const char *key, float defValue)
{
    FILE *file = sOpen(key, true);
    if(!file) {                                         // failed to open settings?
        return defValue;
    }

    int res;
    float val;
    res = fscanf(file, "%f", &val);                     // try to read the value
    fclose(file);

    if(res != 1) {                                      // failed to read value?
        return defValue;
    }

    return val;                                         // return
}

void Settings::setFloat(const char *key, float value)
{
    FILE *file = sOpen(key, false);
    if(!file) {                                         // failed to open settings?
        Debug::out(LOG_ERROR, "Settings::setFloat -- could not write key %s", key);
        return;
    }

    fprintf(file, "%f\n", value);
    fclose(file);
}
//-------------------------
char *Settings::getString(const char *key, const char *defValue)
{
    static char buffer[256];
    memset(buffer, 0, 256);

    FILE *file = sOpen(key, true);
    if(!file) {                                         // failed to open settings?
//      Debug::out(LOG_DEBUG, "Settings::getString -- returning default value for %s", key);
        strcpy(buffer, defValue);
        return buffer;
    }

    char *res;
    res = fgets(buffer, 256, file);                     // try to read the value
    fclose(file);

    if(res == NULL) {                                   // failed to read value?
//      Debug::out(LOG_DEBUG, "Settings::getString -- returning default value for %s", key);
        strcpy(buffer, defValue);
        return buffer;
    }

    return buffer;                                          // return
}

void Settings::setString(const char *key, const char *value)
{
    FILE *file = sOpen(key, false);
    if(!file) {                                         // failed to open settings?
        Debug::out(LOG_ERROR, "Settings::setString -- could not write key %s", key);
        return;
    }

    fputs(value, file);
    fclose(file);
}

//-------------------------
char *Settings::getBinaryString(const char *key, int len)
{
    static char buffer[256];
    memset(buffer, 0, 256);

    char *hexString = getString(key, "INVALID");

    bool good = (strcmp(hexString, "INVALID") != 0);    // if retrieved is something other than the default value, probably valid

    if(good) {                                      // if value seems good, convert it from hex to bin
        char tmp[3];
        int val;

        for(int i=0; i<len; i++) {
            memcpy(tmp, hexString + 2*i, 2);        // copy 2 bytes from value to tmp
            tmp[3] = 0;                             // terminate with zero

            sscanf(tmp, "%X", &val);                // try to read HEX value from tmp
            buffer[i] = val;                        // store at position i
        }
    }

    return buffer;
}
//-------------------------
void Settings::binToHex(uint8_t *inBfr, int len, char *outBfr)
{
    outBfr[0] = 0;                          // put string terminator at the supplied pointer to output buffer, to start output here
    char tmp[16];

    for(int i=0; i<len; i++) {
        sprintf(tmp, "%02X", inBfr[i]);     // get value from inBfr, convert to HEX into tmp
        strcat(outBfr, tmp);                // append to existing content
    }
}
//-------------------------
void Settings::generateLicenseKeyName(uint8_t* hwSerial, char *keyName)
{
    strcpy(keyName, "HW_LICENSE_");                             // start with "HW_LICENSE_"
    Settings::binToHex(hwSerial, 13, keyName + 11);    // take hwSerial and convert it from binary to hex string and append it to key name
}
//-------------------------
void Settings::setBinaryString(const char *key, uint8_t *inBfr, int len)
{
    char tmp[512];
    memset(tmp, 0, 512);

    binToHex(inBfr, len, tmp);  // binary to hex text string
    setString(key, tmp);        // store hex text string as normal string
}
//-------------------------
char Settings::getChar(const char *key, char defValue)
{
    FILE *file = sOpen(key, true);
    if(!file) {                                         // failed to open settings?
//        Debug::out(LOG_DEBUG, "Settings::getChar -- returning default value for %s", key);
        return defValue;
    }

    int res;
    char val;
    res = fscanf(file, "%c", &val);                     // try to read the value
    fclose(file);

    if(res != 1) {                                      // failed to read value?
//        Debug::out(LOG_DEBUG, "Settings::getChar -- returning default value for %s", key);
        return defValue;
    }

    return val;                                         // return
}

void Settings::setChar(const char *key, char value)
{
    FILE *file = sOpen(key, false);
    if(!file) {                                         // failed to open settings?
        Debug::out(LOG_ERROR, "Settings::setChar -- could not write key %s", key);
        return;
    }

    fputc(value, file);
    fclose(file);
}
//-------------------------
FILE *Settings::sOpen(const char *key, bool readNotWrite)
{
    std::string settingsDir = Utils::dotEnvValue("SETTINGS_DIR", "./settings"); // path to settings dir
    std::string path = Utils::mergeHostPaths3(settingsDir, key);

    FILE *file;

    if(readNotWrite) {
        file = fopen(path.c_str(), "r");                // try to open file for reading
    } else {
        file = fopen(path.c_str(), "w");                // try to open file for writing
    }

    return file;
}
