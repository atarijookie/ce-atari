// vim: expandtab shiftwidth=4 tabstop=4
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../libacsiscsi/acsi.h"
#include "../libacsiscsi/hdd_if.h"
#include "../libacsiscsi/find_ce.h"
#include "main.h"
#include "hostmoddefs.h"
#include "keys.h"
#include "defs.h"

uint8_t getKey(void);
uint8_t atariKeysToSingleByte(uint8_t vkey, uint8_t key);

// ------------------------------------------------------------------ 

uint8_t deviceID;
uint8_t commandShort[CMD_LENGTH_SHORT]	= {	0, 'C', 'E', HOSTMOD_CONFIG, CFG_CMD_SET_CFGVALUE, 0};

uint16_t buffer[512/2];

void showComError(void);

// ------------------------------------------------------------------ 
int main( int argc, char* argv[] )
{
    int i;

	// write some header out
	(void) Clear_home();
	(void) Cconws("\33p[  CosmosEx HDD IMAGE TTP  ]\r\n[    by nanard 2016     ]\33q\r\n\r\n");
    
    char *params        = (char *) argv;                            // get pointer to params (path to file)
    int paramsLength    = (int) params[0];
    char *path          = params + 1;
   
    if(paramsLength == 0) {
        (void) Cconws("This is a drap-and-drop, tool\r\n"
                      "to set HDD Image.\r\n\r\n"
                      "\33pArgument is path to HDD image.\33q\r\n\r\n");
        getKey();
        return 0;
    }

    path[paramsLength]  = 0;                                        // terminate path
    (void) Cconws(path);
    (void) Cconws("\r\n");
    
	// search for CosmosEx on ACSI bus
    uint8_t res = findDevice(FIND_DEV_CE);

    if(res == DEVICE_NOT_FOUND) {
        sleep(3);
        return 0;
    }

    deviceID = res & 0x07;                                  // store the BUS ID of device
 
	// now set up the acsi command bytes so we don't have to deal with this one anymore 
	commandShort[0] = (deviceID << 5); 					            // cmd[0] = ACSI_id + TEST UNIT READY (0)	

    char * cbuffer = (char *)buffer;
    memcpy(buffer, "CECFG1", 6);
    i = 6;
    cbuffer[i++] = CFGVALUE_TYPE_ST_PATH;
    memcpy(cbuffer+i, "HDDIMAGE", 9);
    i += 9;
    cbuffer[i++] = strlen(path) + 1;
    memcpy(cbuffer + i, path, strlen(path) + 1);

    uint8_t ret = Supexec(ce_acsiWriteBlockCommand);

    if(ret != 0) {
        (void)Cconws("\r\n***FAILED***\r\n");
    } else {
        (void)Cconws("\r\nOK. \33pPlease RESET\33q\r\n");
    }
    getKey();
	return 0;		
}

#if 0
void intToStr(int val, char *str)
{
    int i3, i2, i1;
    i3 = (val / 100);               // 123 / 100 = 1
    i2 = (val % 100) / 10;          // (123 % 100) = 23, 23 / 10 = 2
    i1 = (val % 10);                // 123 % 10 = 3

    str[0] = i3 + '0';
    str[1] = i2 + '0';
    str[2] = i1 + '0';

    if(val < 100) {
        str[0] = ' ';
    }
    
    if(val < 10) {
        str[1] = ' ';
    }
    
    str[3] = 0;                     // terminating zero
}
#endif

void removeLastPartUntilBackslash(char *str)
{
	int i, len;
	
	len = strlen(str);
	
	for(i=(len-1); i>= 0; i--) {
		if(str[i] == '\\') {
			break;
		}
	
		str[i] = 0;
	}
}

uint8_t ce_acsiWriteBlockCommand(void)
{
	commandShort[0] = (deviceID << 5); 											// cmd[0] = ACSI_id + TEST UNIT READY (0)	
  
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, (uint8_t *)buffer, 1);	// issue the command and check the result 

    if(!hdIf.success) {
        return 0xff;
    }
    
	return hdIf.statusByte;
}

void showComError(void)
{
    (void) Clear_home();
    (void) Cconws("Error in CosmosEx communication!\r\n");
    Cnecin();
}

void createFullPath(char *fullPath, char *filePath, char *fileName)
{
    strcpy(fullPath, filePath);
	
	removeLastPartUntilBackslash(fullPath);				// remove the search wildcards from the end

	if(strlen(fullPath) > 0) {							
		if(fullPath[ strlen(fullPath) - 1] != '\\') {	// if the string doesn't end with backslash, add it
			strcat(fullPath, "\\");
		}
	}
	
    strcat(fullPath, fileName);							// add the filename
}
