#ifndef __UTILS_H__
#define __UTILS_H__

#include <arduino.h>
#include "defs.h"

uint16_t getWord(uint8_t *bfr);
uint32_t getDword(uint8_t *bfr);
uint32_t get24bits(uint8_t *bfr);
void storeWord(uint8_t *bfr, uint16_t val);
void storeDword(uint8_t *bfr, uint32_t val);
void store24bits(uint8_t *bfr, uint32_t val);
void storeHeader(uint8_t *bfr, uint16_t atnCode, uint32_t txLen);

extern uint32_t timerEndMillis;
extern volatile bool hasTimedOut;
void timeoutClear(void);
void timeoutStart(uint32_t durationMs = CMD_TIMEOUT_SHORT);
void longTimeout_basedOnSectorCount(uint16_t sectorCount);
void cmdTimeoutChangeLength(uint32_t newPeriod);

#define SETTINGS_MAGIC  0xcafebabe

struct __attribute__((packed)) TSettings
{
    uint32_t magic;
    uint8_t mac[8];
    uint8_t enabledIDs;
    uint8_t ikbdEnabled;
};

extern TSettings settings;
void loadSettings(void);
void saveSettings(void);

void i2c1init(void);
void debugInit(void);
void debug(const char *fmt, ...);

#ifdef LOG_LED
void debugFromQueue(void);
#endif

void dump_gpio(uint gpio);

#endif
