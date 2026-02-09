// #include "tusb.h"
#include <stdio.h>
#include <stdlib.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>
#include "hardware/spi.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "pico/flash.h"
#include "pico/multicore.h"

#include "defs.h"
#include "utils.h"
#include "psram.h"
#include "display.h"

extern Settings_t Settings;

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

void serialShowMenu(void)
{
    printf("Serial config mode. Current stored settings are:\n");
    printf("--------------------------------------\n");
    printf("SSID    : %s\n", Settings.ssid);
    printf("password: %s\n", Settings.password);
    printf("ikbd    : %s\n", Settings.ikbdEnabled ? "enabled" : "disabled");
    printf("--------------------------------------\n");
    printf("Press 'S' to set SSID, 'P' to set password, 'I' to enable/disable IKBD, 'Q' to save.\n");
}

void serialConfigLoop(void)
{
    bool ssidChanged = false, pswdChanged = false, ikbdChanged = false;

    displayMessage("CONFIG MODE");

    loadSettingsFromEeprom();
    serialShowMenu();

    // main run loop
    while (1) {
        int key = getchar_timeout_us(10);

        if(key == '\n' || key == '\r') {
            serialShowMenu();
        }

        if(key == 's' || key == 'S') {
            printf("\nEnter SSID, finish by Enter key.\n");
            getString(Settings.ssid, MAX_SETTINGS_STRING_LEN);
            printf("\nNew SSID: %s\n", Settings.ssid);
            ssidChanged = true;
        }

        if(key == 'p' || key == 'P') {
            printf("\nEnter password, finish by Enter key.\n");
            getString(Settings.password, MAX_SETTINGS_STRING_LEN);
            printf("\nNew password: %s\n", Settings.password);
            pswdChanged = true;
        }

        if(key == 'i' || key == 'I') {
            printf("\nEnter 'E' to enable IKBD, 'D' to disable IKBD.\n");
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

            printf("\nIKBD: %s\n", Settings.ikbdEnabled ? "enabled" : "disabled");
        }

        // on Q key
        if(key == 'q' || key == 'Q') {
            break;
        }
    }

    // if no settings change is detected, just quit
    if(!ssidChanged && !pswdChanged && !ikbdChanged) {
        printf("No settings changed.\n\n");
        return;
    }

    // some setting was changed, we must store it
    saveSettingsToPSRAM();      // store setting to PSRAM
    psramConfigFlagSet();       // set the flag that we should store data into EEPROM

    debug("RESETing!\n");
    sleep_ms(20);
    watchdog_reboot(0, 0, 0);
}

void storeSettingsFromPSRAMtoEEPROM(void)
{
    debug("Loading settings from PSRAM.\n");
    loadSettingsFromPSRAM();

    debug("Saving settings to EEPROM.\n\n");
    Settings.isValid = SETTINGS_VALID;  // load the isValid flag with the magic number
    saveSettingsToEeprom();

    debug("RESETing!\n");
    sleep_ms(20);
    watchdog_reboot(0, 0, 0);
}
