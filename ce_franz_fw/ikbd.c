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

void circularInit(TCircBuffer *cb)
{
    BYTE i;

    cb->count = 0;              // buffer now empty

    cb->pAdd = &cb->data[0];    // 'add' pointer on start
    cb->pGet = &cb->data[0];    // 'get' pointer on start

    // fill data with zeros
    for(i=0; i<CIRCBUFFER_SIZE; i++) {
        cb->data[i] = 0;
    }
}

__attribute__( ( always_inline ) ) void cicrularAdd(TCircBuffer *cb, BYTE val)
{
    // intentionally removed check of cb->count, as full buffer will fail by not storing (with check) or with overwrite (without check)
    cb->count++;

    // store data at the right position
    *cb->pAdd = val;
    cb->pAdd++;

    if(cb->pAdd >= &cb->data[CIRCBUFFER_SIZE]) {    // if reached end of buffer
        cb->pAdd = &cb->data[0];                    // go to start of buffer
    }
}

__attribute__( ( always_inline ) ) BYTE cicrularGet(TCircBuffer *cb)
{
    BYTE val;

    // intentionally removed check of cb->count, as that is checked where circularGet() is used
    cb->count--;

    // buffer not empty, get data
    val = *cb->pGet;
    cb->pGet++;

    if(cb->pGet >= &cb->data[CIRCBUFFER_SIZE]) {    // if reached end of buffer
        cb->pGet = &cb->data[0];                    // go to start of buffer
    }

    // return value from buffer
    return val;
}
