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

void timerSetup_cmdTimeoutChangeLength(uint32_t newPeriod)
{
    timeoutStartMillis = millis();
    timeoutDuration = newPeriod;
}

void getSetting(uint8_t settingId, uint8_t* settingBfr, uint8_t settingMaxLen)
{
    digitalWrite(PIN_CMD_DATA, HIGH);       // ikbd/eeprom chip to command mode
    delay(3);                               // give some time to stabilize

    // before issuing command, read any data that's in serial waiting to be read and ignore them
    while(Serial1.available() > 0) {
        Serial1.read();
    }

    // send the READ setting command, starting with \n to make sure it's at the start of line
    Serial1.print("\nR");
    Serial1.write(settingId);
    Serial1.print("\n");

    uint32_t start = millis();

    // read from serial, store to buffer
    int i = 0;
    while(true)
    {
        // we're at the end of buffer? quit
        if(i >= (settingMaxLen - 1)) {
            break;
        }

        // taking too long and no more chars comming in? quit
        if((millis() - start) > 100) {
            break;
        }

        // something to read? read and store
        if(Serial1.available() > 0) {
            uint8_t data = Serial1.read();
  
            if(data == '\n') {          // end of line? terminate and quit
                settingBfr[i] = 0;
                break;
            }

            settingBfr[i] = data;        // not end of line, just store
            i++;
        }
    }

    settingBfr[i] = 0;                  // terminate at the end of string
    digitalWrite(PIN_CMD_DATA, LOW);    // ikbd/eeprom chip to data mode
}

void setSetting(uint8_t settingId, uint8_t* settingBfr, uint8_t settingMaxLen)
{
    digitalWrite(PIN_CMD_DATA, HIGH);       // ikbd/eeprom chip to command mode
    delay(3);                               // give some time to stabilize

    // send the WRITE setting command, starting with \n to make sure it's at the start of line
    Serial1.print("\nW");
    Serial1.write(settingId);

    for(int i=0; i<settingMaxLen; i++) {
        if(settingBfr[i] == 0 || settingBfr[i] == '\n') {   // current char is zero or EOL? don't send anything more
            break;
        }

        Serial1.write(settingBfr[i]);   // send setting value
    }

    Serial1.print("\n");                // send EOL to terminate the command

    delay(10);                          // give some time to finish writing
    digitalWrite(PIN_CMD_DATA, LOW);    // ikbd/eeprom chip to data mode
}
