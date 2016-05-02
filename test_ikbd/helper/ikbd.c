#include "ikbd.h"
#include <mint/osbind.h> 
#include "../global.h"
#include "../stdlib.h"

extern void ikbdwc( void );
extern void ikbdget( void );
extern void ikbdtxready( void );
extern BYTE ikbdtxdata;
extern BYTE ikbdrxdata;
extern volatile BYTE ikbdtimeoutflag;

volatile BYTE *pIkbdCtrl = (BYTE *) 0xfffffc00;
volatile BYTE *pIkbdData = (BYTE *) 0xfffffc02;

volatile BYTE *pMfpMaskB = (volatile BYTE *) 0xfffffa15;

BYTE ikbd_putc(BYTE  val);
BYTE ikbd_getc(BYTE *val);

/*
 Transfer a stream of bytes to IKBD 
 @todo:check if it's recieved. Currently ikbdwc() only checks TX being ready. Is this the same? E.g. if CE is down and doesn't relay?
 */
BYTE ikbd_puts( const BYTE* ikbdData, int len ){
	int  i;
    BYTE res = FALSE;
    
	for(i=0; i<len; i++) {
        res = ikbd_putc(ikbdData[i]);   // put out a byte
        
        if(!res) {                      // failed? stop sending stuff
            break;
        }
	}
    
	return res;                         // return true / false
}

BYTE ikbd_put(const BYTE data){
	ikbdtxdata=data;
	ikbdwc();
	if(ikbdtimeoutflag!=0 ){
		return FALSE;
	}
	return TRUE;
}

BYTE ikbd_get(BYTE* retval){
	ikbdget();
	if(ikbdtimeoutflag!=0 ){
		return FALSE;
	}
	*retval=ikbdrxdata;
	return TRUE;
}

BYTE ikbd_gets(BYTE *outString, int len) {
    int i;
    BYTE res = FALSE;
    
    for(i=0; i<len; i++) {
        res = ikbd_getc(&outString[i]); // get one byte
        
        if(!res) {                      // failed? quit
            break;
        }
    }
    
    return res;                         // return true / false
}

BYTE ikbd_txready(void){
	ikbdtxready();
	if((BYTE)ikbdtimeoutflag==(BYTE)0 ){
		return TRUE;
	}
	return FALSE;
}

void ikbd_disable_irq(void)
{
	*pMfpMaskB &= ~(1 << 6);        // remove bit 6 (keyboard / midi)
}

void ikbd_enable_irq(void)
{
	*pMfpMaskB |= (1 << 6);         // add bit 6 (keyboard / midi)
}

BYTE ikbd_putc(BYTE val)
{
    DWORD to = getTicks() + 200;    // 1 second time out
    
    while(1) {
        DWORD now = getTicks();
        if(now >= to) {             // if time out, fail
            return FALSE;
        }
        
        BYTE ctrl = *pIkbdCtrl;
        if(ctrl & 0x02) {           // TXE bit set? quit waiting, send
            break;
        }
    }
    
    *pIkbdData = val;               // send the data
    return TRUE;
}

BYTE ikbd_getc(BYTE *val)
{
    DWORD to = getTicks() + 200;    // 1 second time out
    
    while(1) {
        DWORD now = getTicks();
        if(now >= to) {             // if time out, fail
            return FALSE;
        }
        
        BYTE ctrl = *pIkbdCtrl;
        if(ctrl & 0x01) {           // RXF bit set? quit waiting, receive
            break;
        }
    }
    
    *val = *pIkbdData;              // get the data
    return TRUE;
}
