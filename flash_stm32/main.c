/*
  stm32flash - Open Source ST STM32 flash program for *nix
  Copyright (C) 2010 Geoffrey McRae <geoff@spacevs.com>
..Copyright (C) 2011 Steve Markgraf <steve@steve-m.de>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "utils.h"
#include "serial.h"
#include "stm32.h"
#include "parser.h"

#include "parsers/binary.h"
#include "parsers/hex.h"

#include "gpio.h"

/* device globals */
serial_t	*serial		= NULL;
stm32_t		*stm		= NULL;

void		*p_st		= NULL;
parser_t	*parser		= NULL;

/* settings */
char		*device		= NULL;
serial_baud_t	baudRate	= SERIAL_BAUD_57600;
int		rd	 	= 0;
int		wr		= 0;
int		wu		= 0;
int		npages		= 0xFF;
char		verify		= 0;
int		retry		= 10;
char		init_flag	= 1;
char		force_binary	= 0;
char		*filename;

extern parser_t PARSER_HEX;
extern parser_t PARSER_BINARY;

/* functions */
int  parse_options(int argc, char *argv[]);
void show_help(char *name);

bool flashHans, flashFranz;				// influenced by -x and -y
void outDebugString(const char *format, ...);

int main(int argc, char* argv[]) {
	int ret = 1;
	parser_err_t perr;

	printf("stm32flash - http://stm32flash.googlecode.com/\n\n");
	
	if(!gpio_open()) {							// open RPi GPIO
		return 0;
	}
	
	if (parse_options(argc, argv) != 0) {		// parse arguments
		gpio_close();
		return 0;
	}
	
	if(!flashHans && !flashFranz) {				// user didn't specify if he wants to flash Franz or Hanz?
		fprintf(stderr, "You didn't specify if you want to flash Hans (-x) or Franz (-y).\n");
		gpio_close();
		return 0;
	}

	//----------------------------
	// this was added for CosmosEx
	// select the right STM32 chip 
	bcm2835_gpio_write(PIN_RESET_HANS,			LOW);		// both Hans and Frans to RESET state
	bcm2835_gpio_write(PIN_RESET_FRANZ,			LOW); 

	bcm2835_gpio_write(PIN_BOOT0_FRANZ_HANS,	HIGH);		// BOOT0: L means boot from flash, H means boot the boot loader
	
	// Now hold reset down until the serial port is open and setup, 
	// because that will create some pulse on TX line which confuses the STM32 bootloader.
	//----------------------------
	
	if (wr) {
		/* first try hex */
		if (!force_binary) {
			parser = &PARSER_HEX;
			p_st = parser->init();
			if (!p_st) {
				fprintf(stderr, "%s Parser failed to initialize\n", parser->name);
				goto close;
			}
		}

		if (force_binary || (perr = parser->open(p_st, filename, 0)) != PARSER_ERR_OK) {
			if (force_binary || perr == PARSER_ERR_INVALID_FILE) {
				if (!force_binary) {
					parser->close(p_st);
					p_st = NULL;
				}

				/* now try binary */
				parser = &PARSER_BINARY;
				p_st = parser->init();
				if (!p_st) {
					fprintf(stderr, "%s Parser failed to initialize\n", parser->name);
					goto close;
				}
				perr = parser->open(p_st, filename, 0);
			}

			/* if still have an error, fail */
			if (perr != PARSER_ERR_OK) {
				fprintf(stderr, "%s ERROR: %s\n", parser->name, parser_errstr(perr));
				if (perr == PARSER_ERR_SYSTEM) perror(filename);
				goto close;
			}
		}

		fprintf(stdout, "Using Parser : %s\n", parser->name);
	} else {
		parser = &PARSER_BINARY;
		p_st = parser->init();
		if (!p_st) {
			fprintf(stderr, "%s Parser failed to initialize\n", parser->name);
			goto close;
		}
	}

	serial = serial_open(device);
	if (!serial) {
		perror(device);
		goto close;
	}

	if (serial_setup(
		serial,
		baudRate,
		SERIAL_BITS_8,
		SERIAL_PARITY_EVEN,
		SERIAL_STOPBIT_1
	) != SERIAL_ERR_OK) {
		perror(device);
		goto close;
	}


	//---------------------------------------------------
	// this was added for CosmosEx
	usleep(10000);											// this will assure the RESET line down for at least 10 ms
	
	if(flashHans) {											// flash Hans?
		bcm2835_gpio_write(PIN_TX_SEL1N2,		LOW);		// TX_SEL1N2, HIGH means TX1, LOW means TX2; tx1 is TX FRANZ, tx2 is TX HANS
		bcm2835_gpio_write(PIN_RESET_HANS,		HIGH);		// put Hans into RUN state
	} else {												// flash Franz?
		bcm2835_gpio_write(PIN_TX_SEL1N2,		HIGH);		// TX_SEL1N2, HIGH means TX1, LOW means TX2; tx1 is TX FRANZ, tx2 is TX HANS
		bcm2835_gpio_write(PIN_RESET_FRANZ,		HIGH);		// put Franz into RUN state
	}

	usleep(260000);											// let the STM32 boot and run for 250 + 10 ms (because DS1818 might cause 250 ms RESET signal)
	//---------------------------------------------------	
	
	printf("Serial Config: %s\n", serial_get_setup_str(serial));
	if (!(stm = stm32_init(serial, init_flag))) goto close;

	printf("Version      : 0x%02x\n", stm->bl_version);
	printf("Option 1     : 0x%02x\n", stm->option1);
	printf("Option 2     : 0x%02x\n", stm->option2);
	printf("Device ID    : 0x%04x (%s)\n", stm->pid, stm->dev->name);
	printf("RAM          : %dKiB  (%db reserved by bootloader)\n", (stm->dev->ram_end - 0x20000000) / 1024, stm->dev->ram_start - 0x20000000);
	printf("Flash        : %dKiB (sector size: %dx%d)\n", (stm->dev->fl_end - stm->dev->fl_start ) / 1024, stm->dev->fl_pps, stm->dev->fl_ps);
	printf("Option RAM   : %db\n", stm->dev->opt_end - stm->dev->opt_start);
	printf("System RAM   : %dKiB\n", (stm->dev->mem_end - stm->dev->mem_start) / 1024);

	uint8_t		buffer[256];
	uint32_t	addr;
	unsigned int	len;
	int		failed = 0;

	if (rd) {
		printf("\n");

		if ((perr = parser->open(p_st, filename, 1)) != PARSER_ERR_OK) {
			fprintf(stderr, "%s ERROR: %s\n", parser->name, parser_errstr(perr));
			if (perr == PARSER_ERR_SYSTEM) perror(filename);
			goto close;
		}

		addr = stm->dev->fl_start;
		fprintf(stdout, "\x1B[s");
		fflush(stdout);
		while(addr < stm->dev->fl_end) {
			uint32_t left	= stm->dev->fl_end - addr;
			len		= sizeof(buffer) > left ? left : sizeof(buffer);
			if (!stm32_read_memory(stm, addr, buffer, len)) {
				fprintf(stderr, "Failed to read memory at address 0x%08x, target write-protected?\n", addr);
				goto close;
			}
			assert(parser->write(p_st, buffer, len) == PARSER_ERR_OK);
			addr += len;

			fprintf(stdout,
				"\x1B[uRead address 0x%08x (%.2f%%) ",
				addr,
				(100.0f / (float)(stm->dev->fl_end - stm->dev->fl_start)) * (float)(addr - stm->dev->fl_start)
			);
			fflush(stdout);
		}
		fprintf(stdout,	"Done.\n");
		ret = 0;
		goto close;

	} else if (wu) {
		fprintf(stdout, "Write-unprotecting flash\n");
		/* the device automatically performs a reset after the sending the ACK */
		stm32_wunprot_memory(stm);
		fprintf(stdout,	"Done.\n");

	} else if (wr) {
		printf("\n");

		off_t 	offset = 0;
		ssize_t r;
		unsigned int size = parser->size(p_st);

		if (size > stm->dev->fl_end - stm->dev->fl_start) {
			fprintf(stderr, "File provided larger then available flash space.\n");
			goto close;
		}

		stm32_erase_memory(stm, npages);

		addr = stm->dev->fl_start;
		fprintf(stdout, "\x1B[s");
		fflush(stdout);
		while(addr < stm->dev->fl_end && offset < size) {
			uint32_t left	= stm->dev->fl_end - addr;
			len		= sizeof(buffer) > left ? left : sizeof(buffer);
			len		= len > size - offset ? size - offset : len;

			if (parser->read(p_st, buffer, &len) != PARSER_ERR_OK)
				goto close;
	
			again:
			if (!stm32_write_memory(stm, addr, buffer, len)) {
				fprintf(stderr, "Failed to write memory at address 0x%08x\n", addr);
				goto close;
			}

			if (verify) {
				uint8_t compare[len];
				if (!stm32_read_memory(stm, addr, compare, len)) {
					fprintf(stderr, "Failed to read memory at address 0x%08x\n", addr);
					goto close;
				}

				for(r = 0; r < len; ++r)
					if (buffer[r] != compare[r]) {
						if (failed == retry) {
							fprintf(stderr, "Failed to verify at address 0x%08x, expected 0x%02x and found 0x%02x\n",
								(uint32_t)(addr + r),
								buffer [r],
								compare[r]
							);
							goto close;
						}
						++failed;
						goto again;
					}

				failed = 0;
			}

			addr	+= len;
			offset	+= len;

			fprintf(stdout,
				"\x1B[uWrote %saddress 0x%08x (%.2f%%) ",
				verify ? "and verified " : "",
				addr,
				(100.0f / size) * offset
			);
			fflush(stdout);

		}

		fprintf(stdout,	"Done.\n");
		ret = 0;
		goto close;
	} else
		ret = 0;

close:
	//----------------------------
	// this was added for CosmosEx
	bcm2835_gpio_write(PIN_RESET_HANS,			LOW);		// both Hans and Frans to RESET state
	bcm2835_gpio_write(PIN_RESET_FRANZ,			LOW); 

	bcm2835_gpio_write(PIN_BOOT0_FRANZ_HANS,	LOW);		// BOOT0: L means boot from flash, H means boot the boot loader

	usleep(10000);											// 10 ms pause to let the RESET work

	bcm2835_gpio_write(PIN_RESET_HANS,			HIGH);		// both Hans and Frans to RUN state
	bcm2835_gpio_write(PIN_RESET_FRANZ,			HIGH); 
	
	gpio_close();
	//----------------------------

	if (p_st  ) parser->close(p_st);
	if (stm   ) stm32_close  (stm);
	if (serial) serial_close (serial);

	printf("\n");
	return ret;
}

int parse_options(int argc, char *argv[]) {
	int c;
	while((c = getopt(argc, argv, "b:r:w:e:vn:g:fchuxy")) != -1) {
		switch(c) {
			case 'x':		// flash Hans?
				flashHans	= true;
				flashFranz	= false;
				break;

			case 'y':		// flash Franz?
				flashHans	= false;
				flashFranz	= true;
				break;
				
			case 'b':
				baudRate = serial_get_baud(strtoul(optarg, NULL, 0));
				if (baudRate == SERIAL_BAUD_INVALID) {
					fprintf(stderr,	"Invalid baud rate, valid options are:\n");
					for(baudRate = SERIAL_BAUD_1200; baudRate != SERIAL_BAUD_INVALID; ++baudRate)
						fprintf(stderr, " %d\n", serial_get_baud_int(baudRate));
					return 1;
				}
				break;

			case 'r':
			case 'w':
				rd = rd || c == 'r';
				wr = wr || c == 'w';
				if (rd && wr) {
					fprintf(stderr, "ERROR: Invalid options, can't read & write at the same time\n");
					return 1;
				}
				filename = optarg;
				break;
			case 'e':
				npages = strtoul(optarg, NULL, 0);
				if (npages > 0xFF || npages < 0) {
					fprintf(stderr, "ERROR: You need to specify a page count between 0 and 255");
					return 1;
				}
				break;
			case 'u':
				wu = 1;
				if (rd || wr) {
					fprintf(stderr, "ERROR: Invalid options, can't write unprotect and read/write at the same time\n");
					return 1;
				}
				break;
			case 'v':
				verify = 1;
				break;

			case 'n':
				retry = strtoul(optarg, NULL, 0);
				break;

			case 'f':
				force_binary = 1;
				break;

			case 'c':
				init_flag = 0;
				break;

			case 'h':
				show_help(argv[0]);
				return 1;
		}
	}

	for (c = optind; c < argc; ++c) {
		if (device) {
			fprintf(stderr, "ERROR: Invalid parameter specified\n");
			show_help(argv[0]);
			return 1;
		}
		device = argv[c];
	}

	if (device == NULL) {
		fprintf(stderr, "ERROR: Device not specified\n");
		show_help(argv[0]);
		return 1;
	}

	if (!wr && verify) {
		fprintf(stderr, "ERROR: Invalid usage, -v is only valid when writing\n");
		show_help(argv[0]);
		return 1;
	}

	return 0;
}

void show_help(char *name) {
	fprintf(stderr,
		"Usage: %s [-bvngfhc] [-[rw] filename] /dev/ttyS0\n"
		"	-x 		flash Hans\n"
		"	-y 		flash Franz\n"
		"	-b rate		Baud rate (default 57600)\n"
		"	-r filename	Read flash to file\n"
		"	-w filename	Write flash to file\n"
		"	-u		Disable the flash write-protection\n"
		"	-e n		Only erase n pages before writing the flash\n"
		"	-v		Verify writes\n"
		"	-n count	Retry failed writes up to count times (default 10)\n"
		"	-f		Force binary parser\n"
		"	-h		Show this help\n"
		"	-c		Resume the connection (don't send initial INIT)\n"
		"			*Baud rate must be kept the same as the first init*\n"
		"			This is useful if the reset fails\n"
		"\n"
		"Examples:\n"
		"	Get device information:\n"
		"		%s /dev/ttyS0\n"
		"\n"
		"	Write with verify:\n"
		"		%s -w filename -v /dev/ttyS0\n"
		"\n"
		"	Read flash to file:\n"
		"		%s -r filename /dev/ttyS0\n"
		"\n"
		"	Start execution:\n"
		"		%s -g 0x0 /dev/ttyS0\n",
		name,
		name,
		name,
		name,
		name
	);
}

void outDebugString(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    vprintf(format, args);
	printf("\n");

    va_end(args);
}