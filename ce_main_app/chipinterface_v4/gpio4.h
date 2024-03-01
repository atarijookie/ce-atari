#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdint.h>

#ifndef ONPC
// if compiling for RPi

#include <bcm2835.h>

// assuming we're having P1 connector in version 2 (V2)
// http://www.airspayce.com/mikem/bcm2835/group__constants.html

// pins for both STM32 programming
#define PIN_RESET_FRANZ         RPI_V2_GPIO_P1_15
#define PIN_TXD                 RPI_V2_GPIO_P1_8
#define PIN_RXD                 RPI_V2_GPIO_P1_10
#define PIN_BOOT0_FRANZ         RPI_V2_GPIO_P1_22

// pins for communication with Franz
#define PIN_ATN_FRANZ           RPI_V2_GPIO_P1_24
#define PIN_CS_FRANZ            RPI_V2_GPIO_P1_26
#define PIN_MOSI                RPI_V2_GPIO_P1_19
#define PIN_MISO                RPI_V2_GPIO_P1_21
#define PIN_SCK                 RPI_V2_GPIO_P1_23

#define SPI_CS_FRANZ            BCM2835_SPI_CS1
#define SPI_ATN_FRANZ           PIN_ATN_FRANZ

// direct ACSI connection in ChipInterface v4
#define CMD1ST      RPI_V2_GPIO_P1_37
#define INT_TRIG    RPI_V2_GPIO_P1_31
#define DRQ_TRIG    RPI_V2_GPIO_P1_33
#define FF12D       RPI_V2_GPIO_P1_29
#define EOT         RPI_V2_GPIO_P1_32
#define OUT_OE      RPI_V2_GPIO_P1_07
#define DATA0       RPI_V2_GPIO_P1_36
#define DATA1       RPI_V2_GPIO_P1_11
#define DATA2       RPI_V2_GPIO_P1_12
#define DATA3       RPI_V2_GPIO_P1_35
#define DATA4       RPI_V2_GPIO_P1_38
#define DATA5       RPI_V2_GPIO_P1_40
#define DATA6       RPI_V2_GPIO_P1_16
#define DATA7       RPI_V2_GPIO_P1_18

// To turn off the power to device, set this pin to output and set output level to H.
// For normal operation of device leave this as input.
#define DEVICE_OFF_H    RPI_V2_GPIO_P1_03

#endif      // end of ONPC

bool gpio4_open(void);
void gpio4_close(void);

#endif      // end of _GPIO_H_
