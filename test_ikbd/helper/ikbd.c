#include "ikbd.h"
#include <mint/osbind.h> 
#include "../../libacsiscsi/global.h"
#include "../../libacsiscsi/stdlib.h"

extern void ikbdwc( void );
extern void ikbdget( void );
extern void ikbdtxready( void );
extern uint8_t ikbdtxdata;
extern uint8_t ikbdrxdata;
extern volatile uint8_t ikbdtimeoutflag;

volatile uint8_t *pIkbdCtrl = (uint8_t *) 0xfffffc00;
volatile uint8_t *pIkbdData = (uint8_t *) 0xfffffc02;

volatile uint8_t *pMfpMaskB = (volatile uint8_t *) 0xfffffa15;

uint8_t ikbd_putc(uint8_t  val);
uint8_t ikbd_getc(uint8_t *val);

/*
 Transfer a stream of bytes to IKBD 
 @todo:check if it's recieved. Currently ikbdwc() only checks TX being ready. Is this the same? E.g. if CE is down and doesn't relay?
 */
uint8_t ikbd_puts( const uint8_t* ikbdData, int len ){
	int  i;
    uint8_t res = FALSE;
    
	for(i=0; i<len; i++) {
        res = ikbd_putc(ikbdData[i]);   // put out a byte
        
        if(!res) {                      // failed? stop sending stuff
            break;
        }
	}
    
	return res;                         // return true / false
}

uint8_t ikbd_put(const uint8_t data){
	ikbdtxdata=data;
	ikbdwc();
	if(ikbdtimeoutflag!=0 ){
		return FALSE;
	}
	return TRUE;
}

uint8_t ikbd_get(uint8_t* retval){
	ikbdget();
	if(ikbdtimeoutflag!=0 ){
		return FALSE;
	}
	*retval=ikbdrxdata;
	return TRUE;
}

uint8_t ikbd_gets(uint8_t *outString, int len) {
    int i;
    uint8_t res = FALSE;
    
    for(i=0; i<len; i++) {
        res = ikbd_getc(&outString[i]); // get one byte
        
        if(!res) {                      // failed? quit
            break;
        }
    }
    
    return res;                         // return true / false
}

uint8_t ikbd_txready(void){
	ikbdtxready();
	if((uint8_t)ikbdtimeoutflag==(uint8_t)0 ){
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

uint8_t ikbd_putc(uint8_t val)
{
    uint32_t to = getTicks() + 200;    // 1 second time out
    
    while(1) {
        uint32_t now = getTicks();
        if(now >= to) {             // if time out, fail
            return FALSE;
        }
        
        uint8_t ctrl = *pIkbdCtrl;
        if(ctrl & 0x02) {           // TXE bit set? quit waiting, send
            break;
        }
    }
    
    *pIkbdData = val;               // send the data
    return TRUE;
}

uint8_t ikbd_getc(uint8_t *val)
{
    uint32_t to = getTicks() + 200;    // 1 second time out
    
    while(1) {
        uint32_t now = getTicks();
        if(now >= to) {             // if time out, fail
            return FALSE;
        }
        
        uint8_t ctrl = *pIkbdCtrl;
        if(ctrl & 0x01) {           // RXF bit set? quit waiting, receive
            break;
        }
    }
    
    *val = *pIkbdData;              // get the data
    return TRUE;
}
