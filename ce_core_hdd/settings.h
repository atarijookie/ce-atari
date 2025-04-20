#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint8_t acsiIDdevType[8];       // array of device types for each ACSI ID
    uint8_t sdCardAcsiId;           // ACSI ID assigned to SD card
    uint8_t ceddId;                 // id of CE_DD, which will be used to boot CE_DD.PRG

    uint8_t enabledIDbits;          // bit map of which ACSI IDs are enabled

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

    char *getBinaryString(const char *key, int len);
    void  setBinaryString(const char *key, uint8_t *inBfr, int len);

    char getChar(const char *key, char defValue);
    void setChar(const char *key, char value);

    void loadAcsiIDs(AcsiIDinfo *aii, bool useDefaultsIfNoSettings=true);

    void loadFloppyConfig(FloppyConfig *fc);
    void saveFloppyConfig(FloppyConfig *fc);

    static void generateLicenseKeyName(uint8_t* hwSerial, char *keyName);
    static void binToHex(uint8_t *inBfr, int len, char *outBfr);

private:

    FILE *sOpen(const char *key, bool readNotWrite);

    void storeDefaultValues(void);
};

#endif
