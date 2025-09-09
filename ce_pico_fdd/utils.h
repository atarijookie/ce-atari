#ifndef __UTILS_H__
#define __UTILS_H__

#include <arduino.h>

uint16_t getWord(uint8_t *bfr);
uint32_t getDword(uint8_t *bfr);
uint32_t get24bits(uint8_t *bfr);
void storeWord(uint8_t *bfr, uint16_t val);
void storeDword(uint8_t *bfr, uint32_t val);
void store24bits(uint8_t *bfr, uint32_t val);
void storeHeader(uint8_t *bfr, uint16_t atnCode, uint32_t txLen);

extern uint32_t timerEndMillis;
extern volatile uint8_t hasTimedOut;
void timeoutClear(void);
void timeoutStart(uint32_t durationMs);
void cmdTimeoutChangeLength(uint32_t newPeriod);

#define SETTING_SSID 'S'
#define SETTING_PSWD 'P'
#define SETTING_IDS 'I'
void getSetting(uint8_t settingId, uint8_t *settingBfr, uint8_t settingMaxLen);
void setSetting(uint8_t settingId, uint8_t *settingBfr, uint8_t settingMaxLen);

#define SETTINGS_VALID  0xCAFE

#define MAX_SETTINGS_STRING_LEN 32

typedef struct {
    uint16_t isValid;
    char ssid[MAX_SETTINGS_STRING_LEN];
    char password[MAX_SETTINGS_STRING_LEN];
    uint8_t ikbdEnabled;
} Settings_t;

void loadSettingsFromEeprom(void);
void saveSettingsToEeprom(void);
void getSsidAndPassword(String& argSsid, String& argPassword);
void storeSsidAndPassword(String& argSsid, String& argPassword);
bool getIkbdEnabled(void);
void storeIkbdEnabled(bool argIkbdEnabled);

#endif
