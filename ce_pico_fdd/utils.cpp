#include <cstring>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/flash.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/flash.h"
#include "hardware/flash.h"

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

void storeHeader(uint8_t *bfr, uint16_t atnCode, uint32_t txLen)
{
    storeDword(bfr, SYNC_TAG_FDD);  //  0..3: 0xc050fdd0 [COSmOFDD0] (4 bytes)
    storeWord(bfr + 4, atnCode);    //  4..5: ATN code (2 bytes)
    storeDword(bfr + 6, txLen);     //  6..9: txLen (4 bytes)
}

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
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

// This function will be called when it's safe to call flash_range_erase
static void call_flash_range_erase(void *param) {
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
}

// This function will be called when it's safe to call flash_range_program
static void call_flash_range_program(void *param) {
    uint8_t* data = (uint8_t*) param;
    flash_range_program(FLASH_TARGET_OFFSET, data, FLASH_SECTOR_SIZE);
}

void saveSettingsToEeprom(void)
{
    uint8_t buf[FLASH_SECTOR_SIZE];
    memset(buf, 0, FLASH_SECTOR_SIZE);                          // clear 4k
    memcpy(buf, (const uint8_t *) &Settings, sizeof(Settings)); // copy just the settings - about 68 B

    // writing to flash needs other core to be initialized with flash_safe_execute_core_init(), 
    // see serial_config.cpp for more details.

    int rc = flash_safe_execute(call_flash_range_erase, (void*) FLASH_TARGET_OFFSET, UINT32_MAX);
    if(rc != PICO_OK) {
        debug("flash_range_erase failed, settings not stored\n");
        return;
    }

    rc = flash_safe_execute(call_flash_range_program, (void*) buf, UINT32_MAX);
    if(rc != PICO_OK) {
        debug("flash_range_erase failed, settings not stored\n");
        return;
    }
}

uint32_t millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void BIT_INVERT(int pin)
{
    if(BIT_IS_H(pin)) {
        gpio_put(pin, 0);
    } else {
        gpio_put(pin, 1);
    }
}

void debug(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    for(int i=0; i<len; i++) {
        if(buf[i] == '\n') {
            uart_putc_raw(uart0, '\n');
            uart_putc_raw(uart0, '\r');
        } else {
            uart_putc_raw(uart0, buf[i]);
        }
    }
}
