#ifndef _GPIO_RASCSI_H_
#define _GPIO_RASCSI_H_

#include <stdint.h>

#ifndef ONPC
// if compiling for RPi

#include <bcm2835.h>

#define PIN_BIT_MSG         (1 << 23)
#define PIN_BIT_CD          (1 << 24)
#define PIN_BIT_IO          (1 << 25)
#define PIN_MASK_MSG_CD_IO  (PIN_BIT_MSG | PIN_BIT_CD | PIN_BIT_IO)

#define PIN_ACT     RPI_V2_GPIO_P1_07
#define PIN_SEL     RPI_V2_GPIO_P1_13
#define PIN_REQ     RPI_V2_GPIO_P1_15
#define PIN_MSG     RPI_V2_GPIO_P1_16
#define PIN_CD      RPI_V2_GPIO_P1_18
#define PIN_IO      RPI_V2_GPIO_P1_22
#define PIN_DTD     RPI_V2_GPIO_P1_24
#define PIN_TAD     RPI_V2_GPIO_P1_26
#define PIN_ENB     RPI_V2_GPIO_P1_29
#define PIN_IND     RPI_V2_GPIO_P1_31
#define PIN_ATN     RPI_V2_GPIO_P1_35
#define PIN_BSY     RPI_V2_GPIO_P1_37
#define PIN_RST     RPI_V2_GPIO_P1_38
#define PIN_ACK     RPI_V2_GPIO_P1_40

#define DATA0       RPI_V2_GPIO_P1_19
#define DATA1       RPI_V2_GPIO_P1_23
#define DATA2       RPI_V2_GPIO_P1_32
#define DATA3       RPI_V2_GPIO_P1_33
#define DATA4       RPI_V2_GPIO_P1_08
#define DATA5       RPI_V2_GPIO_P1_10
#define DATA6       RPI_V2_GPIO_P1_36
#define DATA7       RPI_V2_GPIO_P1_11
#define DATAP       RPI_V2_GPIO_P1_12

#define PIN_SDA     RPI_V2_GPIO_P1_03
#define PIN_SCL     RPI_V2_GPIO_P1_05

#endif      // end of ONPC

bool gpiorascsi_open(void);
void gpiorascsi_close(void);

#endif      // end of _GPIO_H_
