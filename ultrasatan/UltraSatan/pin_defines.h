
/////////////////////////////////////////////////////////
// ACSI signals located at Async memory port
#define aCS							(1<<0)
#define aRW							(1<<1)
#define aA1							(1<<2)
#define aACK						(1<<3)
#define aRESET					(1<<4)

// CARD pins located at Async memory port
#define CARD_INS0				(1<<5)
#define CARD_INS1				(1<<6)
/////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////
// ACSI signals located at PF
#define aIRQ						(1<<0)
#define aDRQ						(1<<1)

// SPI CS signals located at PF
#define DATAFLASH				(1<<2)
#define CARD_CS0				(1<<3)
#define CARD_CS1				(1<<4)

#define	SPI_CS_PINS			(DATAFLASH | CARD_CS0 | CARD_CS1)

// RTC signals
#define	RTC_SCL					(1<<5)
#define RTC_SDA					(1<<7)

#define	CARD_CHANGE			(1<<6)

// ACSI data located on PF
// PF8 - PF15 == ACSI DATA
#define aDATA_MASK			0xff00

#define	PF_OUTPUTS			(SPI_CS_PINS | aIRQ | aDRQ)
/////////////////////////////////////////////////////////



