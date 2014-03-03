#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <gem.h>

#include "translated.h"
#include "acsi.h"

BYTE ce_findId(void);
BYTE ce_identify(void);

#define DMA_BUFFER_SIZE		512

BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
BYTE *pDmaBuffer;

BYTE deviceID;

BYTE commandShort[CMD_LENGTH_SHORT]	= {			0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};

short openVdiWorkstation(void);
void windowPaint(short wndHandle, short vdiHandle, GRECT *pRect);
void ce_getDrivesInfo(char *bfr);
void getDriveLine(int index, char *lines, char *line, int maxLen);

#define WINDOW_WIDGETS		(NAME | CLOSER | MOVER)
#define TEXT_ROW_HEIGHT		10

/* ------------------------------------------------------------------ */
int main( int argc, char* argv[] )
{
	BYTE found;

	/* create buffer pointer to even address */
	pDmaBuffer = &dmaBuffer[2];
	pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */

/*	
	// search for CosmosEx on ACSI bus 
	found = ce_findId();

	if(!found) {
		sleep(3);
		return 0;
	}
*/
	deviceID = 0;
	
	/* now set up the acsi command bytes so we don't have to deal with this one anymore */
	commandShort[0] = (deviceID << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	
	short dummy, menuId, msgBuffer[8];
	short pxy[4] = { 0 };
	short myWndHandle = -1, event;
	char *windowTitle = " CE mounts ";
	DWORD windowTitleDw = (DWORD) windowTitle;
	
	// calculate the desired window height for the work are height of that one
	short desiredWindowHeight;
	wind_calc(WC_BORDER, WINDOW_WIDGETS, 0, 0, 100, 15 * TEXT_ROW_HEIGHT, &dummy, &dummy, &dummy, &desiredWindowHeight);

	short vdiHandle = 0;								// 0 mean not init / error
	
	appl_init();										// init AES
	vdiHandle = openVdiWorkstation(); 					// init VDI
   
   	menuId  =  menu_register( gl_apid, "  CE mounts  " );

	while(1) {
		// Go to sleep until an event of interest wakes us, we want menu event messages
		// 	    evnt_multi	(short Type, short Clicks, short WhichButton, short WhichState,
		event = evnt_multi	(MU_MESAG | MU_TIMER, 1,1,1,
				//		short EnterExit1, short In1X, short In1Y, short In1W, short In1H,
						0, pxy[0], pxy[1], 0, 0,
				//		short EnterExit2, short In2X, short In2Y, short In2W, short In2H,
						0,0,0,0,0,
				//		short MesagBuf[], unsigned long Interval, short *OutX, short *OutY,
						msgBuffer, 2000, &pxy[2], &pxy[3],
				// 		short *ButtonState, short *KeyState, short *Key, short *ReturnCount); 
						&dummy, &dummy, &dummy, &dummy); 
  
		// if the event is TIMER
		if(event == MU_TIMER && myWndHandle != -1) {						// when got window and timer occured, redraw
			windowPaint(myWndHandle, vdiHandle, (GRECT *) &msgBuffer[4]);
			continue;
		}
  
		// if the even is MESSAGE
		short wndHandle = msgBuffer[3];
  
		// Check the contents of the message buffer
		switch( msgBuffer[0] )
		{
			case AC_OPEN:
				if( msgBuffer[4] ==  menuId ) {							// they clicked on our menu item? 
					if(myWndHandle == -1) {								// our window is NOT created? create and open it
						myWndHandle = wind_create(WINDOW_WIDGETS, 0, 20, 160, desiredWindowHeight);
						
						if(myWndHandle < 0) {							// failed to create a new window
							break;
						}
						
						wind_set(myWndHandle, WF_NAME, (short) (windowTitleDw >> 16), (short) windowTitleDw, 0, 0);
						wind_open(myWndHandle, 0, 20, 160, desiredWindowHeight);
					} else {											// our window IS created? just put it on top
						wind_set(myWndHandle, WF_TOP, 0, 0, 0, 0);
					}
				}
				break;
				
			case WM_CLOSED:							// request to close the window
			case AC_CLOSE:							// request to close accessory
				if(myWndHandle != -1) {				// our window exists? close and delete it
					wind_close(myWndHandle);
					wind_delete(myWndHandle);
					myWndHandle = -1;
				}
				break;
				
			case WM_MOVED:
				if(myWndHandle != wndHandle) {
					break;
				}

				wind_set(myWndHandle, WF_CURRXYWH, msgBuffer[4], msgBuffer[5], msgBuffer[6], msgBuffer[7]);
				break;
			
			case WM_REDRAW:
				if(myWndHandle != wndHandle) {
					break;
				}
			
				windowPaint(wndHandle, vdiHandle, (GRECT *) &msgBuffer[4]);
				break;
			
			case WM_TOPPED:
				if(myWndHandle != wndHandle) {
					break;
				}

				wind_set(wndHandle, WF_TOP, 0, 0, 0, 0);
				break;
		}
	}
	
	return 0;		
}

short openVdiWorkstation(void)
{
	short paramsIn[16];
	short paramsOut[57];
	short handle;
	
	memset(paramsIn, 0, 16 * sizeof(short));
	memset(paramsOut, 0, 57 * sizeof(short));
	
	paramsIn[0]		= 1;			// current resolution
	paramsIn[10]	= 2;			// raster coordinates
	
	v_opnvwk(paramsIn, &handle, paramsOut); 
	return handle;
}

void windowPaint(short wndHandle, short vdiHandle, GRECT *pRect)
{
	int y, row;
	short wax, way, waw, wah;
    short rectArray[4];
	short wchar, hchar, wcell, hcell;
	char driveLines[1024];
	char driveLine[128];
	
	if(vdiHandle == 0) {							// still not good? quit
		return;
	}
	
	ce_getDrivesInfo(driveLines);
	
	// get work area of this window
	wind_calc(WC_WORK, WINDOW_WIDGETS, pRect->g_x, pRect->g_y, pRect->g_w, pRect->g_h, &wax, &way, &waw, &wah);
	
	// convert struct to array
	rectArray[0] = wax;
	rectArray[1] = way;
	rectArray[2] = wax + waw - 1;
	rectArray[3] = way + wah - 1;
	
	// mouse off, window update begin
	graf_mouse(M_OFF, 0x0L);
	wind_update(BEG_UPDATE);
	 
	// set clipping, clear window
	vs_clip(vdiHandle, 1, rectArray);							// set clipping area
	v_bar(vdiHandle, rectArray);								// draw rectangle
	vst_color(vdiHandle, 1);									// set font color
	vst_height(vdiHandle, 6, &wchar, &hchar, &wcell, &hcell );	// set font height
	
	for(row=0; row<14; row++) {
		y = (row + 1) * 10;
		
		getDriveLine(row, driveLines, driveLine, 128);		
		v_gtext(vdiHandle, wax + 5, way + y, driveLine);
	}
	
	vs_clip(vdiHandle, 0, rectArray);
	
	// stop updating window, show mouse
	wind_update(END_UPDATE);
	graf_mouse(M_ON, 0x0L);
}

void getDriveLine(int index, char *lines, char *line, int maxLen)
{
	int len, cnt=0, i;
	
	len = strlen(lines);

	for(i=0; i<len; i++) {								// find starting position
		if(cnt == index) {								// if got the right count on '\n', break
			break;
		}
	
		if(lines[i] == '\n') {							// found '\n'? Update counter
			cnt++;
		}
	}
	
	if(i == len) {										// index not found?
		line[0] = 0;
		return;
	}
	
	int j;
	for(j=0; j<maxLen; j++) {
		if(lines[i] == 0 || lines[i] == '\n') {
			break;
		}
	
		line[j] = lines[i];
		i++;
	}
	line[j] = 0;										// terminate the string
}

void ce_getDrivesInfo(char *bfr)
{
	strcpy(bfr, "C: USB drive\nD:\nE:\nF:\nG:\nH:\nI:\nJ:\nK:\nL:\nM:\nN:\nO:\nP: shared drive\n");
}

/* this function scans the ACSI bus for any active CosmosEx translated drive */
BYTE ce_findId(void)
{
	char bfr[2], res, i;

	bfr[1] = 0;
	deviceID = 0;
	
	(void) Cconws("Looking for CosmosEx: ");

	for(i=0; i<8; i++) {
		bfr[0] = i + '0';
		(void) Cconws(bfr);

		deviceID = i;									/* store the tested ACSI ID */
		res = Supexec(ce_identify);  					/* try to read the IDENTITY string */
		
		if(res == 1) {                           		/* if found the CosmosEx */
			(void) Cconws("\r\nCosmosEx found on ACSI ID: ");
			bfr[0] = i + '0';
			(void) Cconws(bfr);

			return 1;
		}
	}

	/* if not found */
    (void) Cconws("\r\nCosmosEx not found on ACSI bus, not installing driver.");
	return 0;
}

/* send an IDENTIFY command to specified ACSI ID and check if the result is as expected */
BYTE ce_identify(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	commandShort[4] = TRAN_CMD_IDENTIFY;
  
	memset(pDmaBuffer, 0, 512);              									/* clear the buffer */

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	/* issue the command and check the result */

	if(res != OK) {                        										/* if failed, return FALSE */
		return 0;
	}

	if(strncmp((char *) pDmaBuffer, "CosmosEx translated disk", 24) != 0) {		/* the identity string doesn't match? */
		return 0;
	}
	
	return 1;                             										/* success */
}
