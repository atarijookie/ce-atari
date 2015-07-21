
/*
 * Copyright (c) 2006-2012 by Roland Riegel <feedback@roland-riegel.de>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include <string.h>
#include <iom16v.h>
#include <macros.h>
#include "global.h"
#include "sd_raw.h"

/* card type state */
BYTE sd_raw_card_type;

/* private helper functions */
void sd_raw_send_byte(BYTE b);
BYTE sd_raw_rec_byte(void);
BYTE sd_raw_send_command(BYTE command, DWORD arg);

/**
 * \ingroup sd_raw
 * Initializes memory card communication.
 *
 * \returns 0 on failure, 1 on success.
 */
BYTE sd_raw_init(void)
{
    WORD i;
    BYTE response;

    /* enable outputs for MOSI, SCK, SS, input for MISO */
    configure_pin_mosi();
    configure_pin_sck();
    configure_pin_ss();
    configure_pin_miso();

    unselect_card();

    /* initialize SPI with lowest frequency; max. 400kHz during identification mode of card */
    SPCR = (0 << SPIE) | /* SPI Interrupt Enable */
           (1 << SPE)  | /* SPI Enable */
           (0 << DORD) | /* Data Order: MSB first */
           (1 << MSTR) | /* Master mode */
           (0 << CPOL) | /* Clock Polarity: SCK low when idle */
           (0 << CPHA) | /* Clock Phase: sample on rising SCK edge */
           (1 << SPR1) | /* Clock Frequency: f_OSC / 128 */
           (1 << SPR0);
    SPSR &= ~(1 << SPI2X); /* No doubled clock frequency */

    /* initialization procedure */
    sd_raw_card_type = 0;
    
    /* card needs 74 cycles minimum to start up */
    for(i = 0; i < 10; ++i)
    {
        /* wait 8 clock cycles */
        sd_raw_rec_byte();
    }

    /* address card */
    select_card();

    /* reset card */
    for(i = 0; ; ++i)
    {
        response = sd_raw_send_command(CMD_GO_IDLE_STATE, 0);
        if(response == (1 << R1_IDLE_STATE))
            break;

        if(i == 0x1ff)
        {
            unselect_card();
            return 0;
        }
    }

    /* check for version of SD card specification */
    response = sd_raw_send_command(CMD_SEND_IF_COND, 0x100 /* 2.7V - 3.6V */ | 0xaa /* test pattern */);
    if((response & (1 << R1_ILL_COMMAND)) == 0)
    {
        sd_raw_rec_byte();
        sd_raw_rec_byte();
        if((sd_raw_rec_byte() & 0x01) == 0)
            return 0; /* card operation voltage range doesn't match */
        if(sd_raw_rec_byte() != 0xaa)
            return 0; /* wrong test pattern */

        /* card conforms to SD 2 card specification */
        sd_raw_card_type |= (1 << SD_RAW_SPEC_2);
    }
    else
    {
        /* determine SD/MMC card type */
        sd_raw_send_command(CMD_APP, 0);
        response = sd_raw_send_command(CMD_SD_SEND_OP_COND, 0);
        if((response & (1 << R1_ILL_COMMAND)) == 0)
        {
            /* card conforms to SD 1 card specification */
            sd_raw_card_type |= (1 << SD_RAW_SPEC_1);
        }
        else
        {
            /* MMC card */
        }
    }

    /* wait for card to get ready */
    for(i = 0; ; ++i)
    {
        if(sd_raw_card_type & ((1 << SD_RAW_SPEC_1) | (1 << SD_RAW_SPEC_2)))
        {
            DWORD arg = 0;

            if(sd_raw_card_type & (1 << SD_RAW_SPEC_2))
                arg = 0x40000000;

            sd_raw_send_command(CMD_APP, 0);
            response = sd_raw_send_command(CMD_SD_SEND_OP_COND, arg);
        }
        else
        {
            response = sd_raw_send_command(CMD_SEND_OP_COND, 0);
        }

        if((response & (1 << R1_IDLE_STATE)) == 0)
            break;

        if(i == 0x7fff)
        {
            unselect_card();
            return 0;
        }
    }

    if(sd_raw_card_type & (1 << SD_RAW_SPEC_2))
    {
        if(sd_raw_send_command(CMD_READ_OCR, 0))
        {
            unselect_card();
            return 0;
        }

        if(sd_raw_rec_byte() & 0x40)
            sd_raw_card_type |= (1 << SD_RAW_SPEC_SDHC);

        sd_raw_rec_byte();
        sd_raw_rec_byte();
        sd_raw_rec_byte();
    }

    /* set block size to 512 bytes */
    if(sd_raw_send_command(CMD_SET_BLOCKLEN, 512))
    {
        unselect_card();
        return 0;
    }

    /* deaddress card */
    unselect_card();

    /* switch to highest SPI frequency possible */
    SPCR &= ~((1 << SPR1) | (1 << SPR0)); /* Clock Frequency: f_OSC / 4 */
    SPSR |= (1 << SPI2X); /* Doubled Clock Frequency: f_OSC / 2 */

    return 1;
}

/**
 * \ingroup sd_raw
 * Sends a raw byte to the memory card.
 *
 * \param[in] b The byte to sent.
 * \see sd_raw_rec_byte
 */
void sd_raw_send_byte(BYTE b)
{
    SPDR = b;
    /* wait for byte to be shifted out */
    while(!(SPSR & (1 << SPIF)));
    SPSR &= ~(1 << SPIF);
}

/**
 * \ingroup sd_raw
 * Receives a raw byte from the memory card.
 *
 * \returns The byte which should be read.
 * \see sd_raw_send_byte
 */
BYTE sd_raw_rec_byte(void)
{
    /* send dummy data for receiving some */
    SPDR = 0xff;
    while(!(SPSR & (1 << SPIF)));
    SPSR &= ~(1 << SPIF);

    return SPDR;
}

/**
 * \ingroup sd_raw
 * Send a command to the memory card which responses with a R1 response (and possibly others).
 *
 * \param[in] command The command to send.
 * \param[in] arg The argument for command.
 * \returns The command answer.
 */
BYTE sd_raw_send_command(BYTE command, DWORD arg)
{
    BYTE response;
    BYTE i;

    /* wait some clock cycles */
    sd_raw_rec_byte();

    /* send command via SPI */
    sd_raw_send_byte(0x40 | command);
    sd_raw_send_byte((arg >> 24) & 0xff);
    sd_raw_send_byte((arg >> 16) & 0xff);
    sd_raw_send_byte((arg >> 8) & 0xff);
    sd_raw_send_byte((arg >> 0) & 0xff);
    switch(command)
    {
        case CMD_GO_IDLE_STATE:
           sd_raw_send_byte(0x95);
           break;
        case CMD_SEND_IF_COND:
           sd_raw_send_byte(0x87);
           break;
        default:
           sd_raw_send_byte(0xff);
           break;
    }
    
    /* receive response */
    for(i = 0; i < 10; ++i)
    {
        response = sd_raw_rec_byte();
        if(response != 0xff)
            break;
    }

    return response;
}

/**
 * \ingroup sd_raw
 * Reads raw data from the card.
 *
 * \param[in] offset The offset from which to read.
 * \param[out] buffer The buffer into which to write the data.
 * \param[in] length The number of bytes to read.
 * \returns 0 on failure, 1 on success.
 * \see sd_raw_read_interval, sd_raw_write, sd_raw_write_interval
 */
BYTE sd_raw_read(DWORD sector, BYTE* buffer)
{
    WORD i;
    DWORD address;

    select_card();                      // select SD card

    // for SDHC - use sector #, for SD use address (sector * 512)
    address = (sd_raw_card_type & (1 << SD_RAW_SPEC_SDHC)) ? sector : (sector << 9);

    if( sd_raw_send_command(CMD_READ_SINGLE_BLOCK, address)) {
        unselect_card();
        return 0;
    }

    while(sd_raw_rec_byte() != 0xfe);  //  wait for data block (start byte 0xfe)

    for(i=0; i<512; i++) {
        BYTE b = sd_raw_rec_byte();
        *buffer++ = b;
    }
            
    /* read crc16 */
    sd_raw_rec_byte();
    sd_raw_rec_byte();
            
    /* deaddress card */
    unselect_card();

    /* let card some time to finish */
    sd_raw_rec_byte();

    return 1;
}

BYTE sd_raw_write(DWORD sector, BYTE* buffer)
{
    WORD i;
    DWORD address;

    select_card();                      // select card

    // for SDHC - use sector #, for SD use address (sector * 512)
    address = (sd_raw_card_type & (1 << SD_RAW_SPEC_SDHC)) ? sector : (sector << 9);

    if(sd_raw_send_command(CMD_WRITE_SINGLE_BLOCK, address)) {
        unselect_card();
        return 0;
    }

    sd_raw_send_byte(0xfe);             // send start byte

    for(i=0; i<512; i++) {
        sd_raw_send_byte(*buffer++);
    }

    // write dummy crc16
    sd_raw_send_byte(0xff);
    sd_raw_send_byte(0xff);

    // wait while card is busy
    while(sd_raw_rec_byte() != 0xff);
    sd_raw_rec_byte();

    unselect_card();                    // deselect card

    return 1;
}

/**
 * \ingroup sd_raw
 * Reads informational data from the card.
 *
 * This function reads and returns the card's registers
 * containing manufacturing and status information.
 *
 * \note: The information retrieved by this function is
 *        not required in any way to operate on the card,
 *        but it might be nice to display some of the data
 *        to the user.
 *
 * \param[in] info A pointer to the structure into which to save the information.
 * \returns 0 on failure, 1 on success.
 */
BYTE sd_raw_get_info(struct TSDinfo* info)
{
    WORD  i;
    BYTE  csd_read_bl_len   = 0;
    BYTE  csd_c_size_mult   = 0;
    DWORD csd_c_size        = 0;
    BYTE  csd_structure     = 0;


    if(!info)
        return 0;

    memset(info, 0, sizeof(*info));

    select_card();

    /* read cid register */
    if(sd_raw_send_command(CMD_SEND_CID, 0))
    {
        unselect_card();
        return 0;
    }

    while(sd_raw_rec_byte() != 0xfe);

    for(i=0; i<18; i++)
    {
        BYTE b = sd_raw_rec_byte();

        switch(i)
        {
            case 0:
                info->manufacturer = b;
                break;
            case 1:
            case 2:
                info->oem[i - 1] = b;
                break;
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                info->product[i - 3] = b;
                break;
            case 8:
                info->revision = b;
                break;
            case 9:
            case 10:
            case 11:
            case 12:
                info->serial |= (DWORD) b << ((12 - i) * 8);
                break;
            case 13:
                info->manufacturing_year = b << 4;
                break;
            case 14:
                info->manufacturing_year |= b >> 4;
                info->manufacturing_month = b & 0x0f;
                break;
        }
    }

    /* read csd register */
    if(sd_raw_send_command(CMD_SEND_CSD, 0))
    {
        unselect_card();
        return 0;
    }

    while(sd_raw_rec_byte() != 0xfe);

    for(i = 0; i < 18; ++i)
    {
        BYTE b = sd_raw_rec_byte();

        if(i == 0)
        {
            csd_structure = b >> 6;
        }
        else if(i == 14)
        {
            if(b & 0x40)
                info->flag_copy = 1;
            if(b & 0x20)
                info->flag_write_protect = 1;
            if(b & 0x10)
                info->flag_write_protect_temp = 1;
            info->format = (b & 0x0c) >> 2;
        }
        else
        {
            if(csd_structure == 0x01)
            {
                switch(i)
                {
                    case 7:
                        b &= 0x3f;
                    case 8:
                    case 9:
                        csd_c_size <<= 8;
                        csd_c_size |= b;
                        break;
                }
                if(i == 9)
                {
                    ++csd_c_size;
                    info->capacity = (DWORD) csd_c_size * 1024;
                }
            }
            else if(csd_structure == 0x00)
            {
                switch(i)
                {
                    case 5:
                        csd_read_bl_len = b & 0x0f;
                        break;
                    case 6:
                        csd_c_size = b & 0x03;
                        csd_c_size <<= 8;
                        break;
                    case 7:
                        csd_c_size |= b;
                        csd_c_size <<= 2;
                        break;
                    case 8:
                        csd_c_size |= b >> 6;
                        ++csd_c_size;
                        break;
                    case 9:
                        csd_c_size_mult = b & 0x03;
                        csd_c_size_mult <<= 1;
                        break;
                    case 10:
                        csd_c_size_mult |= b >> 7;

                        info->capacity = (DWORD) csd_c_size << (csd_c_size_mult + csd_read_bl_len + 2);
                        info->capacity = info->capacity >> 9;           // bytes to sectors
                        break;
                }
            }
        }
    }

    unselect_card();

    return 1;
}

