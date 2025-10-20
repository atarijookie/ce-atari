/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "pico/async_context_threadsafe_background.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

#include "uart_rx.pio.h"
#include "defs.h"

#define FIFO_SIZE 64

static PIO pioUart;
static uint smUart;
static int8_t pioIrqUart;
static uint offsetUart;
queue_t fifoUart;

// IRQ called when the pio fifo is not empty, i.e. there are some characters on the uart
static void pio_uart_irq_func(void)
{
    while(!pio_sm_is_rx_fifo_empty(pioUart, smUart)) {
        char c = uart_rx_program_getc(pioUart, smUart);
        queue_try_add(&fifoUart, &c);
    }
}

void pio_uart_setup(void)
{
    // create a queue so the irq can save the data somewhere
    queue_init(&fifoUart, 1, FIFO_SIZE);

    // This will find a free pio and state machine for our program and load it for us
    // We use pio_claim_free_sm_and_add_program_for_gpio_range (for_gpio_range variant)
    // so we will get a PIO instance suitable for addressing gpios >= 32 if needed and supported by the hardware
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_rx_program, &pioUart, &smUart, &offsetUart, PIN_KEYB_RX, 1, true);
    hard_assert(success);

    uart_rx_program_init(pioUart, smUart, offsetUart, PIN_KEYB_RX, 7812);

    // Find a free irq
    pioIrqUart = pio_get_irq_num(pioUart, 0);
    if (irq_get_exclusive_handler(pioIrqUart)) {
        pioIrqUart++;
        if (irq_get_exclusive_handler(pioIrqUart)) {
            panic("All IRQs are in use");
        }
    }

    irq_set_exclusive_handler(pioIrqUart, pio_uart_irq_func);
    irq_set_enabled(pioIrqUart, true); // Enable the IRQ

    const uint irq_index = pioIrqUart - pio_get_irq_num(pioUart, 0); // Get index of the IRQ
    pio_set_irqn_source_enabled(pioUart, irq_index, pio_get_rx_fifo_not_empty_interrupt_source(smUart), true); // Set pio to tell us when the FIFO is NOT empty
}
