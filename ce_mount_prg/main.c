#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../libacsiscsi/acsi.h"
#include "../libacsiscsi/find_ce.h"
#include "../libacsiscsi/hdd_if.h"
#include "translated.h"
#include "keys.h"

#define DMA_BUFFER_SIZE		512

uint8_t dmaBuffer[DMA_BUFFER_SIZE + 2];
uint8_t *pDmaBuffer;

char driveLines[1024];

uint8_t deviceID;

uint8_t commandShort[CMD_LENGTH_SHORT]	= {	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};

void ce_getDrivesInfo(void);
void getDriveLine(int index, char *lines, char *line, int maxLen);

uint8_t getKey(void);
uint8_t atariKeysToSingleByte(uint8_t vkey, uint8_t key);

void showMounts(void);
void showMenu(void);
void unmount(uint8_t drive);
void ce_unmount(void);

/* ------------------------------------------------------------------ */
int main( int argc, char* argv[] )
{
	/* create buffer pointer to even address */
	pDmaBuffer = &dmaBuffer[2];
	pDmaBuffer = (uint8_t *) (((uint32_t) pDmaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */

	// search for CosmosEx on ACSI & SCSI bus
    uint8_t res = findDevice(FIND_DEV_CE | FIND_DEV_CS);

    if(res == DEVICE_NOT_FOUND) {
		sleep(3);
        return 0;
    }

    deviceID = res & 0x07;                                  // store the BUS ID of device
    
	/* now set up the acsi command bytes so we don't have to deal with this one anymore */
	commandShort[0] = (deviceID << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	
    showMenu();
    
    while(1) {
        uint8_t key = getKey();
    
        if(key == KEY_F10) {
            break;
        }
        
        if(key >= 'a' && key <= 'z') {      // to lower case
            key = key - 32;
        }
        
        if(key >= 'C' && key <= 'O') {
            unmount(key);
        }
        
        showMenu();
    }
	
	return 0;		
}

void unmount(uint8_t drive)
{
    if(drive < 'C' || drive > 'O') {
        return;
    }
    
    commandShort[5] = drive - 'A';          // contains drive index to unmount
    Supexec(ce_unmount);
}

void showMenu(void)
{
    (void) Clear_home();
    (void) Cconws("\33p[CosmosEx mount tool, by Jookie 2014]\33q\r\n");
    (void) Cconws("Press from C to P to unmount drive.\r\n");
    (void) Cconws("Press F10 to quit.\r\n\r\n");

    showMounts();
}

void showMounts(void)
{
	int row;
	char driveLine[128];
	
	Supexec(ce_getDrivesInfo);
	
	for(row=0; row<14; row++) {
		getDriveLine(row, driveLines, driveLine, 128);		
		(void) Cconws(driveLine);
		(void) Cconws("\r\n");
    }
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

void ce_unmount(void)
{
	commandShort[4] = ACC_UNMOUNT_DRIVE;
//	commandShort[5] = 0;            // fill this before calling this function
	
	// ask for new data
	(*hdIf.cmd)(ACSI_READ, commandShort, 6, pDmaBuffer, 1);
	
	// if failed to get data
	if(!hdIf.success || hdIf.statusByte != OK) {
        (void) Clear_home();
		(void) Cconws("CE connection fail...\n");
        getKey();
	}
}

void ce_getDrivesInfo(void)
{
	commandShort[4] = ACC_GET_MOUNTS;
	commandShort[5] = 0;
	
	// ask for new data
	(*hdIf.cmd)(ACSI_READ, commandShort, 6, pDmaBuffer, 1);
	
	// if failed to get data
	if(!hdIf.success || hdIf.statusByte != OK) {
		strcpy(driveLines, "CE connection fail\n");
		return;
	}
	
	// got data, copy them to the right buffer
	strcpy(driveLines, (char *) pDmaBuffer);
}
