#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "pico/sync.h"

#include <EEPROM.h>

#include "utils.h"
#include "defs.h"
#include "display.h"

#ifdef LOG_LED
#include "uart_tx.pio.h"

PIO pioUartTx;
uint smUartTx;

queue_t fifoDebug;
#endif

volatile bool hasTimedOut = false;
volatile uint8_t timerRunning = false;
alarm_id_t timer_id = 0;    // Will hold the alarm handle

mutex_t debugMutex;

struct TSettings settings;

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

int64_t timer_callback(alarm_id_t id, void *user_data) {
    hasTimedOut = true;
    timerRunning = false;
    return 0;   // 0 = do NOT reschedule (one-shot)
}

void timeoutStart(uint32_t durationMs)
{
// #ifdef LOG_MORE
//     debug("timeoutStart %d at %d", durationMs, millis());
// #endif

    // If already running, cancel old one
    if (timerRunning) {
        timerRunning = false;
        cancel_alarm(timer_id);
    }

    timer_id = add_alarm_in_ms(durationMs, timer_callback, NULL, false);

    hasTimedOut = false; // reset flag
    timerRunning = true;
}

void timeoutClear(void)
{
// #ifdef LOG_MORE
//  debug("timeoutClear\n");
// #endif

    if (timerRunning) {
        timerRunning = false;
        cancel_alarm(timer_id);
    }

    hasTimedOut = false;
    timerRunning = false;
}

void cmdTimeoutChangeLength(uint32_t newPeriod)
{
    timeoutClear();
    timeoutStart(newPeriod);
}

void longTimeout_basedOnSectorCount(uint16_t sectorCount)
{
    uint32_t mbCount       = (sectorCount >> 11) + 1;                  // convert sector count into megabytes (rounded up)
    uint32_t timeoutSecs   = mbCount       * CMD_TIMEOUT_SECS_PER_MB;  // convert MBs into seconds
    uint32_t timeoutPeriod = timeoutSecs   * CMD_TIMEOUT_ONESECOND;    // and convert seconds into ms

    uint32_t timeoutDuration = MIN(timeoutPeriod, 30000);        // Now limit the timeout period to 30 s
    cmdTimeoutChangeLength(timeoutDuration);
}

void storeHeader(uint8_t *bfr, uint16_t atnCode, uint32_t txLen)
{
    storeDword(bfr, 0xc050d1c5); //  0..3: 0xc050d1c5 [COSmODICS] (4 bytes)
    storeWord(bfr + 4, atnCode); //  4..5: ATN code (2 bytes)
    storeDword(bfr + 6, txLen);  //  6..9: txLen (4 bytes)
}

void saveSettings(void)
{
    multicore_lockout_start_blocking();

    settings.magic = SETTINGS_MAGIC;

    uint8_t* pSettings = (uint8_t*) &settings;
    for(int i=0; i<sizeof(settings); i++) {
        EEPROM.write(i, pSettings[i]);
    }

    EEPROM.commit();

    multicore_lockout_end_blocking();
}

void loadSettings(void)
{
    EEPROM.begin(512);

    // read settings from eeprom
    uint8_t* pSettings = (uint8_t*) &settings;
    for(int i=0; i<sizeof(settings); i++) {
        pSettings[i] = EEPROM.read(i);
    }

    // if settings are not stored yet, store default settings
    if(settings.magic != SETTINGS_MAGIC) {
        settings.magic = SETTINGS_MAGIC;
        settings.enabledIDs = 1;

        for(int i=0; i<6; i++) {
            settings.mac[i] = random(255);
        }
        settings.mac[0] = (settings.mac[0] & 0xFE) | 0x02;   // LAA + unicast

        saveSettings();
    }
}

void debugInit(void)
{
    mutex_init(&debugMutex);

#ifdef LOG_UART
    // uart0 for debug strings
    Serial1.begin(115200);
#endif

#ifdef LOG_LED
    uint offset;

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_tx_program, &pioUartTx, &smUartTx, &offset, PIN_LED_EVB, 1, true);
    if(!success) { debug("Failed to claim PIO SM for UART\n"); while(1); }

    uart_tx_program_init(pioUartTx, smUartTx, offset, PIN_LED_EVB, 115200);

    queue_init(&fifoDebug, 1, 1024);
#endif
}

#ifdef LOG_LED
void debugFromQueue(void)
{
    while(!queue_is_empty(&fifoDebug)) {
        char c;
        queue_try_remove(&fifoDebug, &c);
        uart_tx_program_putc(pioUartTx, smUartTx, c);
    }
}
#endif

void debug(const char *fmt, ...)
{
    mutex_enter_blocking(&debugMutex);

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // replace \n with \n\r
    int j=0;
    char buf2[2048];
    for(int i=0; i<len; i++) {
        buf2[j++] = buf[i];

        if(buf[i] == '\n') {
            buf2[j++] = '\r';
        }
    }
    buf2[j] = 0;

#ifdef LOG_UART
    // use uart0 for debug strings
    uart_puts(uart0, buf2);
#endif

#ifdef LOG_LED
    if(get_core_num() == 1)         // core 1 - just add to fifo
    {
        int i = 0;

        while(buf2[i] != 0) {
            queue_try_add(&fifoDebug, &buf2[i]);
            i++;
        }
    }
    else                            // core 0 - actually send to uart
    {
        uart_tx_program_puts(pioUartTx, smUartTx, buf2);
    }
#endif

    mutex_exit(&debugMutex);
}
