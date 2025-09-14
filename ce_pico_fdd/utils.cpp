#include <cstring>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/flash.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/multicore.h"

#include "defs.h"
#include "utils.h"
#include "connection.h"

volatile uint8_t hasTimedOut = false;
// alarm_id_t alarmId = -1;
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

// int64_t onTimer(alarm_id_t id, void *user_data)
// {
//     if(alarmId == id) {
//         hasTimedOut = true;
//     }

//     return 0; // return 0 == one-shot, don’t reschedule
// }

// void timeoutStart(uint32_t durationMs)
// {
// // #ifdef LOG_MORE
// //     printf("timeoutStart %d at %d\n", durationMs, millis());
// // #endif

//     if(alarmId > 0) // if timer running, stop it first
//     {
//         cancel_alarm(alarmId);
//         alarmId = -1;
//     }

//     hasTimedOut = false;
//     alarmId = add_alarm_in_ms(durationMs, onTimer, NULL, true);
// }

// void timeoutClear(void)
// {
// // #ifdef LOG_MORE
// //     printf("timeoutClear");
// // #endif

//     hasTimedOut = false;
//     cancel_alarm(alarmId);
//     alarmId = -1;
// }

// void cmdTimeoutChangeLength(uint32_t newPeriod)
// {
//     timeoutClear();
//     timeoutStart(newPeriod);
// }

void storeHeader(uint8_t *bfr, uint16_t atnCode, uint32_t txLen)
{
    storeDword(bfr, SYNC_TAG_FDD);  //  0..3: 0xc050fdd0 [COSmOFDD0] (4 bytes)
    storeWord(bfr + 4, atnCode);    //  4..5: ATN code (2 bytes)
    storeDword(bfr + 6, txLen);     //  6..9: txLen (4 bytes)
}

#define FLASH_PAGE_SIZE_4K      4096
#define FLASH_TARGET_OFFSET     ((4 * 1024 * 1024) - FLASH_PAGE_SIZE_4K)
const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

// load settings from eeprom into struct
void loadSettingsFromEeprom(void)
{
    memset(&Settings, 0, sizeof(Settings));
    memcpy(&Settings, flash_target_contents, sizeof(Settings));

    if(Settings.isValid != SETTINGS_VALID) {
        memset(&Settings, 0, sizeof(Settings));
    }
}

void saveSettingsToEeprom(void)
{
    uint32_t ints = save_and_disable_interrupts();
    multicore_lockout_start_blocking();

    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_PAGE_SIZE_4K);
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t *) &Settings, sizeof(Settings));

    multicore_lockout_end_blocking();
    restore_interrupts(ints);
}

uint32_t millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}
