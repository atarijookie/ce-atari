#include "mydefines.h"
#include "serial.h"
#include "spi.h"
//-----------------------------------------------------
BYTE SPIstatus;

volatile DWORD SPIbytesToRead;

extern volatile BYTE *bfrIn;
extern volatile WORD GotBytes;

//-----------------------------------------------------
void spiInit(void)
{
  spiCShigh(0xff);									// all CS lines to H
  
	CLEAR(*pSPI_CTL, (1<<14));				// disable SPI

	*pSPI_CTL		= 0x1c05;
	*pSPI_FLG		= 0xff00;							// turn off HW control of CS lines
	*pSPI_STAT	= 0x0056;							// clear sticky status bits
	*pSPI_BAUD	= 0x014c;							// speed: 200 kHz
		
	SET(*pSPI_CTL, (1<<14));					// enable SPI
}
//-----------------------------------------------------
void spiCShigh(BYTE cs)
{
	DWORD which = SPI_CS_PINS;
	
	switch(cs)
	{
		case 0:		which = CARD_CS0; break;
		case 1:		which = CARD_CS1; break;
		case 123:	which = DATAFLASH; break;
		default:	which = SPI_CS_PINS; break;	// if any other value, then ALL CS PINS to H
	}
	
	*pFIO_FLAG_S = which;								// the CS pin to H
}
//-----------------------------------------------------
void spiCSlow(BYTE cs)
{
	DWORD which;
	
	switch(cs)
	{
		case 0:		which = CARD_CS0; break;
		case 1:		which = CARD_CS1; break;
		case 123:	which = DATAFLASH; break;
		default:	return;
	}
	
	*pFIO_FLAG_C = which;								// the CS pin to L
}
//-----------------------------------------------------
void spiSetSPCKfreq(BYTE which)
{
	WORD spi_baud;

	switch(which)									// values valid for SCLK of 133 MHz 					
	{
		case SPI_FREQ_12MHZ:	spi_baud = 0x0006; break;			// 11.083 MHz
		case SPI_FREQ_16MHZ:	spi_baud = 0x0004; break;			// 16.625 MHz
		case SPI_FREQ_22MHZ:	spi_baud = 0x0003; break;			// 22.166 MHz
		
		case SPI_FREQ_SLOW:		spi_baud = 0x014c; break;			//  0.200 MHz
		default:							spi_baud = 0x014c; break;			//  0.200 MHz
	}

	CLEAR(*pSPI_CTL, (1<<14));			// disable SPI
	
	*pSPI_BAUD = spi_baud;					// set the new frequency

	SET(*pSPI_CTL, (1<<14));				// enable SPI
}
//-----------------------------------------------------
void spiSendByte(BYTE data)
{
	WORD stat;
	SPIstatus = TRUE;

  while(1)										// wait while the TX buffer is full
  {
  	stat = *pSPI_STAT;				// read status
  	stat = stat & 0x0008;			// get only TXS bit (SPI_TDBR Data Buffer Status)
  	
  	if(stat==0)								// if TXS==0, TX buffer is not full
  		break;
  }
  
	*pSPI_TDBR = (WORD)data;		// send the data
}
//-----------------------------------------------------
BYTE spiTransferByte(BYTE data)
{
	WORD stat;
  SPIstatus = TRUE;
  BYTE bVal;

  while(1)										// wait while the TX buffer is full
  {
  	stat = *pSPI_STAT;				// read status
  	stat = stat & 0x0008;			// get only TXS bit (SPI_TDBR Data Buffer Status)
  	
  	if(stat==0)								// if TXS==0, TX buffer is not full
  		break;
  }

	*pSPI_TDBR = (WORD)data;		// send the data
 	//-------------
  while(1)										// wait while the RX buffer is not full
  {
  	stat = *pSPI_STAT;				// read status
  	stat = stat & 0x0020;			// get only RXS bit (RX Data Buffer Status)
  	
  	if(stat==0x0020)					// if RXS==1, RX buffer is full
  		break;
  }
	
  bVal = (BYTE) (*pSPI_RDBR);	// read the received data

  return bVal;
}
//-----------------------------------------------------
BYTE spiReceiveByte(void)
{
	WORD stat;
  SPIstatus = TRUE;
  BYTE bVal;

 	//-------------
  while(1)										// wait while the RX buffer is not full
  {
  	stat = *pSPI_STAT;				// read status
  	stat = stat & 0x0020;			// get only RXS bit (RX Data Buffer Status)
  	
  	if(stat==0x0020)					// if RXS==1, RX buffer is full
  		break;
  }
	
  bVal = (BYTE) (*pSPI_RDBR);	// read the received data

  return bVal;
}
//-----------------------------------------------------
WORD spiTransferWord(WORD data)
{
	WORD rxData = 0;

	// send MS byte of given data
	rxData = (spiTransferByte((data>>8) & 0x00FF))<<8;
	
	// send LS byte of given data
	rxData |= (spiTransferByte(data & 0x00FF));

	// return the received data
	return rxData;
}
//-----------------------------------------------------

