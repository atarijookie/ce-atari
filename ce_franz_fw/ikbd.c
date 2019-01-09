#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#include "defs.h"
#include "timers.h"
#include "main.h"
#include "floppyhelpers.h"
#include "init.h"
#include "ikbd.h"
#include "global_vars.h"


// --------------------------------------------------
// UART 1 - TX and RX - connects Franz and RPi, 19200 baud
// UART 2 - TX - data from RPi     - through buff0 - to IKBD
// UART 2 - RX - data from IKBD    - through buff1 - to RPi (also connected with wire to ST keyb)
// UART 3 - RX - data from ST keyb - through buff1 - to RPi
//
// Flow with RPi:
// IKBD talks to ST keyboard (direct wire connection), and also talks through buff1 to RPi
// ST keyb talks through buff1 to RPi
// RPi talks to IKBD through buff0
//
// Flow without RPi:
// IKBD talks to ST keyboard - direct wire connection
// ST keyb talks to IKBD - through buff0
//
// --------------------------------------------------

void USART1_IRQHandler(void)                                            // USART1 is connected to RPi
{
    if((USART1->SR & USART_FLAG_RXNE) != 0) {                           // if something received
        BYTE val = USART1->DR;                                          // read received value
        cicrularAdd(&buff0, val);                                       // add to buffer
    }
}

void USART2_IRQHandler(void)                                            // USART2 is connected to ST IKBD port
{
    if((USART2->SR & USART_FLAG_RXNE) != 0) {                           // if something received
        BYTE val = USART2->DR;                                          // read received value

        if(hostIsUp) {                                                  // RPi is up - buffered send to RPi
            cicrularAdd(&buff1, UARTMARK_STCMD);                        // add to buffer - MARK
            cicrularAdd(&buff1, val);                                   // add to buffer - DATA
        }
    }
}

void USART3_IRQHandler(void)                                            // USART3 is connected to original ST keyboard
{
    if((USART3->SR & USART_FLAG_RXNE) != 0) {                           // if something received
        BYTE val = USART3->DR;                                          // read received value

        if(hostIsUp) {                                                  // RPi is up - buffered send to RPi
            cicrularAdd(&buff1, UARTMARK_KEYBDATA);                     // add to buffer - MARK
            cicrularAdd(&buff1, val);                                   // add to buffer - DATA
        } else {                                                        // if RPi is not up - send it to IKBD (through buffer or immediatelly)
            cicrularAdd(&buff0, val);                                   // add to buffer
        }
    }
}

void circularInit(volatile TCircBuffer *cb)
{
    BYTE i;

    // set vars to zero
    cb->addPos = 0;
    cb->getPos = 0;
    cb->count = 0;

    // fill data with zeros
    for(i=0; i<CIRCBUFFER_SIZE; i++) {
        cb->data[i] = 0;
    }
}

void cicrularAdd(volatile TCircBuffer *cb, BYTE val)
{
    // if buffer full, fail
    if(cb->count >= CIRCBUFFER_SIZE) {
        return;
    }
    cb->count++;

    // store data at the right position
    cb->data[ cb->addPos ] = val;

    // increment and fix the add position
    cb->addPos++;
    cb->addPos = cb->addPos & CIRCBUFFER_POSMASK;
}

BYTE cicrularGet(volatile TCircBuffer *cb)
{
    BYTE val;

    // if buffer empty, fail
    if(cb->count == 0) {
        return 0;
    }
    cb->count--;

    // buffer not empty, get data
    val = cb->data[ cb->getPos ];

    // increment and fix the get position
    cb->getPos++;
    cb->getPos = cb->getPos & CIRCBUFFER_POSMASK;

    // return value from buffer
    return val;
}
