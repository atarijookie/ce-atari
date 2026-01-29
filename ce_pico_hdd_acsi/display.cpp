#include <cstdint>

#include "defs.h"
#include "display.h"
#include "utils.h"

extern volatile uint8_t core1state;
extern volatile bool blinking;

void blinkOnce(uint32_t onTimeMs, uint32_t offTimeMs)
{
    LED_ON;
    sleep_ms(onTimeMs);
    LED_OFF;
    sleep_ms(offTimeMs);
}

// Pico repeating timer
repeating_timer_t timer;

// Timer callback (runs every 100 ms)
bool timerCallback(repeating_timer_t *t)
{
    static int tickCounter = 0;

    if(tickCounter < 1) {
        LED_ON;
    } else {
        LED_OFF;
    }

    tickCounter++;
    if(tickCounter >= 4) {
        tickCounter = 0;
    }

    return true; // keep repeating
}

void blinkingStart(void)
{
    blinking = true;
    add_repeating_timer_ms(100, timerCallback, NULL, &timer);
}

void blinkingStop(void)
{
    if(!blinking) {
        return;
    }

    blinking = false;
    cancel_repeating_timer(&timer);
}
