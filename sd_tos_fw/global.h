#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define BYTE    unsigned char
#define WORD    unsigned short
#define DWORD   unsigned long
	
#define TRUE    1
#define FALSE   0

// PORT A
#define A6          (1 << 7)
#define A5          (1 << 6)
#define A4          (1 << 5)
#define A3          (1 << 4)
#define A1          (1 << 3)
#define A2          (1 << 2)
#define A0          (1 << 1)
#define D0          (1 << 0)

// PORT B
#define SCK         (1 << 7)
#define MISO        (1 << 6)
#define MOSI        (1 << 5)
#define SD_CS       (1 << 4)
#define A1          (1 << 3)
#define D3          (1 << 2)
#define D2          (1 << 1)
#define D1          (1 << 0)

// PORT C
#define A7          (1 << 7)
#define WE          (1 << 6)
#define A8          (1 << 5)
#define OE          (1 << 4)
#define CS          (1 << 3)
#define D7          (1 << 2)
#define SIPO_D      (1 << 1)
#define D6          (1 << 0)

// PORT D
#define D5          (1 << 7)
#define SIPO_OE     (1 << 6)
#define SIPO_CLK    (1 << 5)
#define D4          (1 << 4)
#define BTN         (1 << 3)
#define IS_SLAVE    (1 << 2)
#define DBG_TXT     (1 << 1)
#define ST_RESET    (1 << 0)


#endif


