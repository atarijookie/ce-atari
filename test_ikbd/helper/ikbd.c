#include "ikbd.h"
#include <mint/osbind.h> 
#include "../global.h"


extern void ikbdwc( void );
extern void ikbdget( void );
extern void ikbdtxready( void );
extern BYTE ikbdtxdata;
extern BYTE ikbdrxdata;
extern volatile BYTE ikbdtimeoutflag;

/*
 Transfer a stream of bytes to IKBD 
 @todo:check if it's recieved. Currently ikbdwc() only checks TX being ready. Is this the same? E.g. if CE is down and doesn't relay?
 */
BYTE ikbd_ws( const BYTE* ikbdData, int len ){
	int cnt;
	for( cnt=0;cnt<len;cnt++){
		ikbdtxdata=ikbdData[cnt];
		ikbdwc();
		(void)Cconws("*");
		if(ikbdtimeoutflag!=0 ){
			return FALSE;
		}
	}
	return TRUE;
}

BYTE ikbd_put(BYTE data){
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

BYTE ikbd_txready(){
	ikbdtxready();
	if((BYTE)ikbdtimeoutflag==(BYTE)0 ){
		return TRUE;
	}
	return FALSE;
}

BYTE ikbd_disable_irq(){
	*((volatile BYTE*)0xFFFFFA15)&=0xBF;
}

BYTE ikbd_enable_irq(){
	*((BYTE*)0xFFFFFA15)|=(6<<1);
}