#include "commands.h"

const BYTE ikbd_reset_data[]={0x80,0x01};

void ikbd_reset(){
	BYTE retcode=0;
	ikbd_puts(ikbd_reset_data,2);
	ikbd_get(&retcode);	
}