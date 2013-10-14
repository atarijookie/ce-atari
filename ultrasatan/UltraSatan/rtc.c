//------------------------------------------------------
#include "rtc.h"
#include "serial.h"
#include "main.h"
//------------------------------------------------------
void I2C_H(WORD what)
{
	*pFIO_DIR		&= ~what;									// set as INPUTs
	*pFIO_INEN	|= what;									// add these from input enable
}
//------------------------------------------------------
void I2C_L(WORD what)
{
	*pFIO_DIR		|= what;									// set as OUTPUTs
	*pFIO_INEN	&= ~what;									// remove these from input enable

	*pFIO_FLAG_C = what;									// set to L (pull down to L)
}
//------------------------------------------------------
void rtc_write(BYTE address, BYTE data, BYTE WriteData)
{
	BYTE res;
	
	//------------
	I2C_H(RTC_SCL | RTC_SDA);		
	wait_us(50);
	//------------
	// START
	I2C_L(RTC_SDA);												// SDA to L	
	wait_us(20);
	//------------
	res = rtc_writeByte(0xd0);						// address of RTC & R/W bit
	
//	if(!res)
//		return;
	//------------
	res = rtc_writeByte(address);					// write address
	
//	if(!res)
//		return;
	//------------
	if(WriteData)													// if should write data
	{
		res = rtc_writeByte(data);					// write the data
	
//		if(!res)
//			return;
	}
	//------------
	// STOP
	I2C_L(RTC_SDA | RTC_SCL);							// SDA & SCL are L
	wait_us(20);
	
	I2C_H(RTC_SCL);												// SCL goes H
	wait_us(20);

	I2C_H(RTC_SDA);												// SDA goes H
	wait_us(40);
}
//------------------------------------------------------
BYTE rtc_read(BYTE address)
{
	BYTE res, data;
	
	//------------
	rtc_write(address, 0, 0);							// we have to write address 1st
	//------------
	I2C_H(RTC_SCL | RTC_SDA);							// SDA & SCL to H
	wait_us(50);
	//------------
	// START
	I2C_L(RTC_SDA);												// SDA to L	
	wait_us(20);
	//------------
	res = rtc_writeByte(0xd1);						// address of RTC & R/W bit
	
//	if(!res)
//		return;
	//------------
	data = rtc_readByte(1);								// read the last byte
	
//	if(!res)
//		return;
	//------------
	// STOP
	I2C_L(RTC_SDA | RTC_SCL);							// SDA & SCL are L
	wait_us(20);
	
	I2C_H(RTC_SCL);												// SCL goes H
	wait_us(20);

	I2C_H(RTC_SDA);												// SDA goes H
	wait_us(40);
	
	return data;
}
//------------------------------------------------------
BYTE rtc_writeByte(BYTE data)
{
	BYTE bit;

	//-----------------
	// data writing out
	for(bit=0; bit<8; bit++)
	{
		I2C_L(RTC_SCL);											// SCL to L
		wait_us(20);
		
		if(data & 0x80)											// if data bit is set
			I2C_H(RTC_SDA);										// SDA to H
		else
			I2C_L(RTC_SDA);										// SDA to L
				
		data = data << 1;										// shift data
			
		wait_us(20);												// data setup time before SCL going H
		
		I2C_H(RTC_SCL);											// SCL to H
		wait_us(20);

		I2C_L(RTC_SCL);											// SCL to L
		wait_us(20);
	}		
	//----------------
	I2C_H(RTC_SCL | RTC_SDA);							// SCL to H
	wait_us(20);

	bit = *pFIO_FLAG_D & RTC_SDA;					// read SDA pin (ACKNOWLEDGE)
	
	I2C_L(RTC_SCL);												// SCL to L
	wait_us(20);
	
	if(bit)																// if ACK not preset, failed
		return 0;

	return 1;															// succeeded
}
//------------------------------------------------------
BYTE rtc_readByte(BYTE isLast)
{
	BYTE bit, data, inVal;

	I2C_H(RTC_SDA);		
	//-----------------
	// data reading in
	data = 0;
	
	for(bit=0; bit<8; bit++)
	{
		I2C_L(RTC_SCL);											// SCL to L
		wait_us(20);
		
		I2C_H(RTC_SCL);											// SCL to H
		wait_us(20);
		
		inVal = *pFIO_FLAG_D & RTC_SDA;			// read SDA
		
		data = data << 1;										// shift data
		
		if(inVal)														// if data was on SDA, set
			data |= 0x01;

		I2C_L(RTC_SCL);											// SCL to L
		wait_us(20);
	}		
	//----------------
	if(isLast)														// for last byte - negative ACK
		I2C_H(RTC_SDA);
	else																	// for NOT last byte - ACK
		I2C_L(RTC_SDA);
		
	wait_us(20);
	
	I2C_H(RTC_SCL);												// SCL to H
	wait_us(20);

	I2C_L(RTC_SCL);												// SCL to L
	wait_us(20);
	//----------------

	return data;													// return the read byte
}
//------------------------------------------------------
BYTE Dec2Bin(BYTE dec)
{
	BYTE hi, lo, val;
	
	hi = (dec & 0xf0) >> 4;
	lo = (dec & 0x0f);
	val = hi * 10 + lo;
	
	return val;
}
//------------------------------------------------------
BYTE Bin2Dec(BYTE dec)
{
	BYTE hi, lo, val;
	
	hi = dec / 10;
	lo = dec % 10;
	
	val = (hi<<4) | lo;
	
	return val;
}
//------------------------------------------------------
BYTE rtc_GetClock(BYTE *buffer)
{
	BYTE hou, min, sec, day, mon, yea, halt;
	WORD year;	
	
	//---------------	
	// read the current Date & Time from RTC chip	
	sec = rtc_read(0);
	min = rtc_read(1);
	hou = rtc_read(2);

	day = rtc_read(4);
	mon = rtc_read(5);
	yea = rtc_read(6);

	//---------------	
	// get the HALT CLOCK flag and if the clock is halted, then enable it
	halt = sec & 0x80;

	if(halt)													// if clock halted
		rtc_write(0, 0, 1);							// disable HALT and delete seconds
	//---------------	
	// convert the date & time from decimal to binary format of numbers
	sec = Dec2Bin(sec & 0x7f);
	min = Dec2Bin(min & 0x7f);
	hou = Dec2Bin(hou & 0x3f);
	day = Dec2Bin(day & 0x3f);
	mon = Dec2Bin(mon & 0x1f);
	yea = Dec2Bin(yea & 0xff);
	year = 2000 + yea;
	//---------------	
	// if the caller has passed the pointer to buffer
	if(buffer)
	{	
		buffer[0] = yea;
		buffer[1] = mon;
		buffer[2] = day;
		buffer[3] = hou;
		buffer[4] = min;
		buffer[5] = sec;
		
		return 1;
	}
	//---------------	
	// if the caller did no pass a pointer to buffer, just write it out
	uart_prints("\nDate: ");
	
	fputD(year, NULL);
	uart_putc('-');
	fputD(mon, NULL);
	uart_putc('-');
	fputD(day, NULL);

	uart_prints("\nTime: ");

	fputD(hou, NULL);
	uart_putc(':');
	fputD(min, NULL);
	uart_putc(':');
	fputD(sec, NULL);
	
	uart_prints("\n\n");
	
	return 1;
}
//------------------------------------------------------
BYTE rtc_SetClock(WORD year, BYTE mon, BYTE day, BYTE hou, BYTE min, BYTE sec)
{
	if(year>2100 || mon>12 || day>31 || hou>23 || sec>59)		// invalid values?
		return 0;
	//---------------	
	sec = Bin2Dec(sec);														// convert binary numbers to decimal
	min = Bin2Dec(min);
	hou = Bin2Dec(hou);
	day = Bin2Dec(day);
	mon = Bin2Dec(mon);
	
	if(year > 1999)
		year = year - 2000;
		
	year = Bin2Dec(year);
	//---------------	
	rtc_write(0, sec, 1);
	rtc_write(1, min, 1);
	rtc_write(2, hou, 1);

	rtc_write(4, day, 1);
	rtc_write(5, mon, 1);
	rtc_write(6, (BYTE) year, 1);

	return 1;
}
//------------------------------------------------------
