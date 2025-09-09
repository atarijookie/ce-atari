#include "defs.h"
#include "connection.h"
#include "hardware/timer.h"

volatile uint8_t hasTimedOut = false;
alarm_id_t alarmId = -1;
uint32_t timerEndMillis = 0;

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
