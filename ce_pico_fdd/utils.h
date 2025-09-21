#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <cstdint>

uint16_t getWord(uint8_t *bfr);
uint32_t getDword(uint8_t *bfr);
uint32_t get24bits(uint8_t *bfr);
void storeWord(uint8_t *bfr, uint16_t val);
void storeDword(uint8_t *bfr, uint32_t val);
void store24bits(uint8_t *bfr, uint32_t val);
void storeHeader(uint8_t *bfr, uint16_t atnCode, uint32_t txLen);

extern uint32_t timerEndMillis;
extern volatile uint8_t hasTimedOut;
// void timeoutClear(void);
// void timeoutStart(uint32_t durationMs);
// void cmdTimeoutChangeLength(uint32_t newPeriod);

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
uint32_t millis(void);
void xprintf(const char *format, ...);

void BIT_INVERT(int pin);

#endif
