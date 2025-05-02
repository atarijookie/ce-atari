#include "defs.h"

uint32_t timeoutStartMillis;
uint32_t timeoutDuration;

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

void longTimeout_basedOnSectorCount(uint16_t sectorCount)
{
    uint32_t mbCount       = (sectorCount >> 11) + 1;                  // convert sector count into megabytes (rounded up)
    uint32_t timeoutSecs   = mbCount       * CMD_TIMEOUT_SECS_PER_MB;  // convert MBs into seconds
    uint32_t timeoutPeriod = timeoutSecs   * CMD_TIMEOUT_ONESECOND;    // and convert seconds into ms

    timeoutStartMillis = millis();
    timeoutDuration = MIN(timeoutPeriod, 30000);        // Now limit the timeout period to 30 s
}

void timeoutStart(void)
{
    timeoutStartMillis = millis();
    timeoutDuration = CMD_TIMEOUT_SHORT;
}

uint8_t timeout(void)
{
    uint32_t now = millis();

    if ((now - timeoutStartMillis) > timeoutDuration)
    {
        return TRUE;
    }

    return FALSE;
}

void cmdTimeoutChangeLength(uint32_t newPeriod)
{
    timeoutStartMillis = millis();
    timeoutDuration = newPeriod;
}
