#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"

#include "defs.h"
#include "utils.h"
#include "connection.h"

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

void serialConfigLoop(void)
{
    bool ssidChanged = false, pswdChanged = false, ikbdChanged = false;

    loadSettingsFromEeprom();

    printf("\n\nEntering configuration mode.\nCurrent settings are:\n");
    printf("--------------------------------------\n");
    printf("SSID    : %s\n", Settings.ssid);
    printf("password: %s\n", Settings.password);
    printf("ikbd    : %s\n", Settings.ikbdEnabled ? "enabled" : "disabled");
    printf("--------------------------------------\n");
    printf("Press 'S' to set SSID, 'P' to set password, 'I' to enable/disable IKBD, 'Q' to save.\n");

    while(true)
    {
        int key = getchar_timeout_us(1000);

        if(key == '\n' || key == '\r') {
            printf("Press 'S' to set SSID, 'P' to set password, 'I' to enable/disable IKBD, 'Q' to save.\n");
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


        if(key == 'q' || key == 'Q') {
            break;
        }
    }

    if(!ssidChanged && !pswdChanged && !ikbdChanged) {
        printf("No settings changed.\n\n");
        return;
    }

    printf("Saving settings.\n\n");
    saveSettingsToEeprom();
    watchdog_reboot(0, 0, 0);
}
