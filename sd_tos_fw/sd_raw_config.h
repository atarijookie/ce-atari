
/*
 * Copyright (c) 2006-2012 by Roland Riegel <feedback@roland-riegel.de>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#ifndef SD_RAW_CONFIG_H
#define SD_RAW_CONFIG_H

#define SD_RAW_WRITE_SUPPORT        1
#define SD_RAW_WRITE_BUFFERING      1
#define SD_RAW_SDHC                 1

#define configure_pin_mosi()        DDRB |= (1 << DDB5)
#define configure_pin_sck()         DDRB |= (1 << DDB7)
#define configure_pin_ss()          DDRB |= (1 << DDB4)
#define configure_pin_miso()        DDRB &= ~(1 << DDB6)

#define select_card()               PORTB &= ~(1 << PORTB4)
#define unselect_card()             PORTB |= (1 << PORTB4)

#endif

