#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdio.h>
#include "datatypes.h"

typedef struct {
    BYTE acsiIDdevType[8];                              // array of device types for each ACSI ID
    BYTE sdCardAcsiId;                                  // ACSI ID assigned to SD card

    BYTE enabledIDbits;                                 // bit map of which ACSI IDs are enabled

    bool gotDevTypeRaw;
    bool gotDevTypeTranslated;
    bool gotDevTypeSd;
} AcsiIDinfo;

typedef struct {
    bool enabled;
    int  id;
    bool writeProtected;
    bool soundEnabled;
} FloppyConfig;

class Settings
{
public:
    Settings(void);
    virtual ~Settings(void) { };

    bool getBool(const char *key, bool defValue);
    void setBool(const char *key, bool value);

    int  getInt(const char *key, int defValue);
    void setInt(const char *key, int value);

    float getFloat(const char *key, float defValue);
    void  setFloat(const char *key, float value);

    char *getString(const char *key, const char *defValue);
    void  setString(const char *key, const char *value);

    char getChar(const char *key, char defValue);
    void setChar(const char *key, char value);

    void loadAcsiIDs(AcsiIDinfo *aii, bool useDefaultsIfNoSettings=true);

    void loadFloppyConfig(FloppyConfig *fc);
    void saveFloppyConfig(FloppyConfig *fc);

private:

    FILE *sOpen(const char *key, bool readNotWrite);

    void storeDefaultValues(void);
};

#endif
