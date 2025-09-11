#include <SPI.h>
#include <EEPROM.h>

#include "defs.h"
#include "utils.h"
#include "connection.h"
#include "hardware/timer.h"

volatile uint8_t hasTimedOut = false;
alarm_id_t alarmId = -1;
Settings_t Settings;

uint16_t getWord(uint8_t *bfr)
{
    uint16_t val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get lo

    return val;
}

uint32_t getDword(uint8_t *bfr)
{
    uint32_t val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get mid hi
    val = val << 8;

    val |= bfr[2];      // get mid lo
    val = val << 8;

    val |= bfr[3];      // get lo

    return val;
}

uint32_t get24bits(uint8_t *bfr)
{
    uint32_t val = 0;

    val  = bfr[0];       // get hi
    val  = val << 8;

    val |= bfr[1];      // get mid
    val  = val << 8;

    val |= bfr[2];      // get lo

    return val;
}

void storeWord(uint8_t *bfr, uint16_t val)
{
    bfr[0] = val >> 8;  // store hi
    bfr[1] = val;       // store lo
}

void storeDword(uint8_t *bfr, uint32_t val)
{
    bfr[0] = val >> 24; // store hi
    bfr[1] = val >> 16; // store mid hi
    bfr[2] = val >>  8; // store mid lo
    bfr[3] = val;       // store lo
}

void store24bits(uint8_t *bfr, uint32_t val)
{
    bfr[0] = val >> 16;
    bfr[1] = val >>  8;
    bfr[2] = val;
}

int64_t onTimer(alarm_id_t id, void *user_data)
{
    if(alarmId == id) {
        hasTimedOut = true;
    }

    return 0; // return 0 == one-shot, don’t reschedule
}

void timeoutStart(uint32_t durationMs)
{
// #ifdef LOG_MORE
//     Serial.print("timeoutStart ");
//     Serial.print(durationMs);
//     Serial.print(" at ");
//     Serial.println(millis());
// #endif

    if(alarmId > 0) // if timer running, stop it first
    {
        cancel_alarm(alarmId);
        alarmId = -1;
    }

    hasTimedOut = false;
    alarmId = add_alarm_in_ms(durationMs, onTimer, NULL, true);
}

void timeoutClear(void)
{
// #ifdef LOG_MORE
//     Serial.println("timeoutClear");
// #endif

    hasTimedOut = false;
    cancel_alarm(alarmId);
    alarmId = -1;
}

void cmdTimeoutChangeLength(uint32_t newPeriod)
{
    timeoutClear();
    timeoutStart(newPeriod);
}

void storeHeader(uint8_t *bfr, uint16_t atnCode, uint32_t txLen)
{
    storeDword(bfr, SYNC_TAG_FDD);  //  0..3: 0xc050fdd0 [COSmOFDD0] (4 bytes)
    storeWord(bfr + 4, atnCode);    //  4..5: ATN code (2 bytes)
    storeDword(bfr + 6, txLen);     //  6..9: txLen (4 bytes)
}

// load settings from eeprom into struct
void loadSettingsFromEeprom(void)
{
    EEPROM.begin(256);      // makes a copy of the emulated EEPROM sector in RAM to allow random update and access

    uint8_t* pSettings = (uint8_t*) &Settings;
    for(int i=0; i<sizeof(Settings); i++) {
        pSettings[i] = EEPROM.read(i);
    }

    EEPROM.end();           // frees all memory used
}

void saveSettingsToEeprom(void)
{
    EEPROM.begin(256);      // makes a copy of the emulated EEPROM sector in RAM to allow random update and access
    Settings.isValid = SETTINGS_VALID;

    uint8_t* pSettings = (uint8_t*) &Settings;
    for(int i=0; i<sizeof(Settings); i++) {
        EEPROM.write(i, pSettings[i]);
    }

    EEPROM.commit();        // writes the updated data to flash
    EEPROM.end();           // frees all memory used
}

void getSsidAndPassword(String& argSsid, String& argPassword)
{
    loadSettingsFromEeprom();

    if(Settings.isValid == SETTINGS_VALID) {
        argSsid = String(Settings.ssid);
        argPassword = String(Settings.password);
    } else {
        argSsid = "";
        argPassword = "";
    }
}

void storeSsidAndPassword(String& argSsid, String& argPassword)
{
    loadSettingsFromEeprom();

    if(Settings.isValid != SETTINGS_VALID) {
        Settings.isValid = SETTINGS_VALID;
        Settings.ikbdEnabled = 1;
    }

    strncpy(Settings.ssid, argSsid.c_str(), MIN(argSsid.length(), MAX_SETTINGS_STRING_LEN - 1));
    Settings.ssid[MAX_SETTINGS_STRING_LEN - 1] = 0;

    strncpy(Settings.password, argPassword.c_str(), MIN(argPassword.length(), MAX_SETTINGS_STRING_LEN - 1));
    Settings.password[MAX_SETTINGS_STRING_LEN - 1] = 0;

    saveSettingsToEeprom();
}

bool getIkbdEnabled(void)
{
    loadSettingsFromEeprom();
    return ((Settings.isValid == SETTINGS_VALID) ? Settings.ikbdEnabled : true);
}

void storeIkbdEnabled(bool argIkbdEnabled)
{
    loadSettingsFromEeprom();

    if(Settings.isValid != SETTINGS_VALID) {
        Settings.isValid = SETTINGS_VALID;
        memset(Settings.ssid, 0, MAX_SETTINGS_STRING_LEN);
        memset(Settings.password, 0, MAX_SETTINGS_STRING_LEN);
    }

    Settings.ikbdEnabled = argIkbdEnabled;
    saveSettingsToEeprom();
}
