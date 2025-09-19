#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "pico/flash.h"
#include "pico/multicore.h"

#include "defs.h"
#include "utils.h"
#include "connection.h"

extern Settings_t Settings;

/*
    In order for flashing to work, we must ensure that only 1 core is writing to flash and running, so
    we must enter config mode and storing to flash before we call cyw43_arch_init(), 
    which runs on other core. 

    When we decide to finally write to flash, we start the other core, which needs to execute
    flash_safe_execute_core_init(), so when flash_safe_execute() starts, it can pause the core1.

    After this, we restart pico, don't use core1 directly and init wifi to run on the other core
    for the main code execution.
*/

void core1_entry_for_flashing(void)
{
    flash_safe_execute_core_init();     // call this for flash_safe_execute() to work
    while(1);                           // do nothing until restarted and loaded with some other code
}

void getString(char* buffer, int maxLen)
{
    memset(buffer, 0, maxLen);

    int i=0;
    while(1)
    {
        int key = getchar_timeout_us(1000);

        if(key == PICO_ERROR_TIMEOUT) {
            continue;
        }

        if(key == '\n' || key == '\r' || i >= (maxLen-1)) {
            break;
        }

        putchar(key);       // echo back to console

        buffer[i] = key;
        i++;
    }
}

void serialConfigLoop(void)
{
    bool ssidChanged = false, pswdChanged = false, ikbdChanged = false;

    loadSettingsFromEeprom();

    xprintf("\n\nEntering configuration mode.\nCurrent settings are:\n");
    xprintf("--------------------------------------\n");
    xprintf("SSID    : %s\n", Settings.ssid);
    xprintf("password: %s\n", Settings.password);
    xprintf("ikbd    : %s\n", Settings.ikbdEnabled ? "enabled" : "disabled");
    xprintf("--------------------------------------\n");
    xprintf("Press 'S' to set SSID, 'P' to set password, 'I' to enable/disable IKBD, 'Q' to save.\n");

    while(true)
    {
        int key = getchar_timeout_us(1000);

        if(key == '\n' || key == '\r') {
            xprintf("Press 'S' to set SSID, 'P' to set password, 'I' to enable/disable IKBD, 'Q' to save.\n");
        }

        if(key == 's' || key == 'S') {
            xprintf("\nEnter SSID, finish by Enter key.\n");
            getString(Settings.ssid, MAX_SETTINGS_STRING_LEN);
            xprintf("\nNew SSID: %s\n", Settings.ssid);
            ssidChanged = true;
        }

        if(key == 'p' || key == 'P') {
            xprintf("\nEnter password, finish by Enter key.\n");
            getString(Settings.password, MAX_SETTINGS_STRING_LEN);
            xprintf("\nNew password: %s\n", Settings.password);
            pswdChanged = true;
        }

        if(key == 'i' || key == 'I') {
            xprintf("\nEnter 'E' to enable IKBD, 'D' to disable IKBD.\n");
            char ed[2];
            getString(ed, 2);

            if(ed[0] == 'e' || ed[0] == 'E') {
                Settings.ikbdEnabled = true;
                ikbdChanged = true;
            }

            if(ed[0] == 'd' || ed[0] == 'D') {
                Settings.ikbdEnabled = false;
                ikbdChanged = true;
            }

            xprintf("\nIKBD: %s\n", Settings.ikbdEnabled ? "enabled" : "disabled");
        }


        if(key == 'q' || key == 'Q') {
            break;
        }
    }

    if(!ssidChanged && !pswdChanged && !ikbdChanged) {
        xprintf("No settings changed.\n\n");
        return;
    }

    xprintf("Starting core1 for flashing.\n");
    multicore_launch_core1(core1_entry_for_flashing);

    xprintf("Saving settings.\n\n");
    Settings.isValid = SETTINGS_VALID;  // load the isValid flag with the magic number
    saveSettingsToEeprom();
    watchdog_reboot(0, 0, 0);
}
