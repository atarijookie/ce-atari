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

//--------------

extern BYTE hostIsUp;           // used to just pass through IKBD until RPi is up

volatile TCircBuffer buff0, buff1;
void circularInit(volatile TCircBuffer *cb);
void cicrularAdd(volatile TCircBuffer *cb, BYTE val);
BYTE cicrularGet(volatile TCircBuffer *cb);

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

        if(buff0.count > 0 || (USART2->SR & USART_FLAG_TXE) == 0) {     // got data in buffer or usart2 can't TX right now?
            cicrularAdd(&buff0, val);                                   // add to buffer
            USART2->CR1 |= USART_FLAG_TXE;                              // enable interrupt on USART TXE
        } else {                                                        // if no data in buffer and usart2 can TX
            USART2->DR = val;                                           // send it to USART2
        }
    }

    if((USART1->SR & USART_FLAG_TXE) != 0) {                            // if can TX
        if(buff1.count > 0) {                                           // and there is something to TX
            BYTE val = cicrularGet(&buff1);
            USART1->DR = val;
        } else {                                                        // and there's nothing to TX
            USART1->CR1 &= ~USART_FLAG_TXE;                             // disable interrupt on USART2 TXE
        }
    }
}

#define UARTMARK_STCMD      0xAA
#define UARTMARK_KEYBDATA   0xBB

void USART2_IRQHandler(void)                                            // USART2 is connected to ST IKBD port
{
    if((USART2->SR & USART_FLAG_RXNE) != 0) {                           // if something received
        BYTE val = USART2->DR;                                          // read received value

        if(hostIsUp) {                                                  // RPi is up - buffered send to RPi
            if(buff1.count > 0 || (USART1->SR & USART_FLAG_TXE) == 0) { // got data in buffer or usart1 can't TX right now?
                cicrularAdd(&buff1, UARTMARK_STCMD);                    // add to buffer - MARK
            } else {                                                    // if no data in buffer and usart2 can TX
                USART1->DR = UARTMARK_STCMD;                            // send to USART1 - MARK
            }

            cicrularAdd(&buff1, val);                                   // add to buffer - DATA
            USART1->CR1 |= USART_FLAG_TXE;                              // enable interrupt on USART TXE
        } else {                                                        // if RPi is not up, something received from ST?
            // nothing to do, data should automatically continue to ST keyboard
        }
    }

    if((USART2->SR & USART_FLAG_TXE) != 0) {                            // if can TX
        if(buff0.count > 0) {                                           // and there is something to TX
            BYTE val = cicrularGet(&buff0);
            USART2->DR = val;
        } else {                                                        // and there's nothing to TX
            USART2->CR1 &= ~USART_FLAG_TXE;                             // disable interrupt on USART2 TXE
        }
    }
}

void USART3_IRQHandler(void)                                            // USART3 is connected to original ST keyboard
{
    if((USART3->SR & USART_FLAG_RXNE) != 0) {                           // if something received
        BYTE val = USART3->DR;                                          // read received value

        if(hostIsUp) {                                                  // RPi is up - buffered send to RPi
            if(buff1.count > 0 || (USART1->SR & USART_FLAG_TXE) == 0) { // got data in buffer or usart1 can't TX right now?
                cicrularAdd(&buff1, UARTMARK_KEYBDATA);                 // add to buffer - MARK
            } else {                                                    // if no data in buffer and usart2 can TX
                USART1->DR = UARTMARK_KEYBDATA;                         // send to USART1 - MARK
            }

            cicrularAdd(&buff1, val);                                   // add to buffer - DATA
            USART1->CR1 |= USART_FLAG_TXE;                              // enable interrupt on USART TXE
        } else {                                                        // if RPi is not up - send it to IKBD (through buffer or immediatelly)
            if(buff0.count > 0 || (USART2->SR & USART_FLAG_TXE) == 0) { // got data in buffer or usart2 can't TX right now?
                cicrularAdd(&buff0, val);                               // add to buffer
                USART2->CR1 |= USART_FLAG_TXE;                          // enable interrupt on USART TXE
            } else {                                                    // if no data in buffer and usart2 can TX
                USART2->DR = val;                                       // send it to USART2
            }
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

