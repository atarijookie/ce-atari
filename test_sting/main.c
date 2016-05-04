//--------------------------------------------------
#include <mint/osbind.h> 
#include <stdio.h>

#include "version.h"
#include "global.h"
#include "transprt.h"
#include "stdlib.h"
#include "out.h"

//--------------------------------------------------
DRV_LIST  *sting_drivers;
TPL       *tpl;

#define MAGIC   "STiKmagic"
#define CJTAG   "STiK"

void  getServerIp (void);
DWORD tryToParseIp(char *ipString);

BYTE find_STiNG(void);

void doTest00(void);
void doTest01(void);
void doTest02(void);
void doTest03(void);

BYTE readBuffer [130 * 1024];
BYTE writeBuffer[130 * 1024];

BYTE *rBuf, *wBuf;

#define SERVER_ADDR_DEFAULT_STRING  "192.168.123.142"
#define SERVER_ADDR_DEFAULT_DWORD   0xc0a87b8e

DWORD SERVER_ADDR;

int main(void)
{
    Clear_home();

    initBuffer();

    rBuf = readBuffer;
    wBuf = writeBuffer;

    (void) Cconws("STiNG test...\r\n");

    BYTE found = Supexec (find_STiNG);
    if(!found) {
        sleep(3);
        return 0;
    }

    getServerIp();

    doTest00();
    doTest01();
    doTest02();
    doTest03();

    writeBufferToFile();
    deinitBuffer();
    
    sleep(3);
	return 0;
}

BYTE find_STiNG(void)
{
   DWORD *work;
   sting_drivers = NULL;
   
   for (work = * (DWORD **) 0x5a0L; *work != 0L; work += 2) {
        if (*work == 'STiK') {
            sting_drivers = (DRV_LIST *) *(++work);
            break;
        }
    }
    
    if(sting_drivers == NULL) {
        (void) Cconws("STiNG not found.\r\n");
        return 0;
    }
   
    if(strncmp(sting_drivers->magic, MAGIC, 8) != 0) {
        (void) Cconws("STiNG MAGIC mismatch.\r\n");
        return 0;
    }

    tpl = (TPL *) (*sting_drivers->get_dftab) (TRANSPORT_DRIVER);

    if (tpl == (TPL *) NULL) {
        (void) Cconws("STiNG TRANSPORT layer not found.\r\n");
        return 0;
    }

    (void) Cconws("STiNG found.\r\n");
    return 1;
}

void getServerIp(void)
{
    (void) Cconws("Enter server IP: ");

    char inIp[20];
    memset(inIp, 0, 20);
    inIp[0] = 19;           // offset 0: maximum number of chars allowed to read
    Cconrs(inIp);           // read the string from keyboard
                            // offset 1: number of chars retrieved
                            // offset 2: the retrieved string
                            
    // minimal IP lenght: strlen("1.2.3.4") = 7
    
    if(inIp[1] < 7) {       // if the new IP is too short to be valid, use default
        SERVER_ADDR = SERVER_ADDR_DEFAULT_DWORD;
    } else {
        SERVER_ADDR = tryToParseIp(inIp + 2);
    }
    (void) Cconws("\r\n");
    
    char tmp[100];
    strcpy(tmp, "Server IP      : ");
    
    int ofs;

    ofs = strlen(tmp);
    intToString((SERVER_ADDR >> 24) & 0xff, -1, tmp + ofs);
    strcat(tmp, ".");

    ofs = strlen(tmp);
    intToString((SERVER_ADDR >> 16) & 0xff, -1, tmp + ofs);
    strcat(tmp, ".");

    ofs = strlen(tmp);
    intToString((SERVER_ADDR >>  8) & 0xff, -1, tmp + ofs);
    strcat(tmp, ".");

    ofs = strlen(tmp);
    intToString((SERVER_ADDR      ) & 0xff, -1, tmp + ofs);
    
    out_s(tmp);
}

DWORD tryToParseIp(char *ipString)
{
    DWORD ip = 0;
    int i;
    
    for(i=0; i<4; i++) {
        int len = strlen_ex(ipString, '.');         // find out length of ip number (limited by '.' or 0)
        
        if(len < 1 || len > 3) {                    // wrong length? return default
            return SERVER_ADDR_DEFAULT_DWORD;
        }
        
        int val = getIntFromStr(ipString, len);     // read integer from string
        if(val < 0 || val > 255) {                  // number out of range? return default
            return SERVER_ADDR_DEFAULT_DWORD;
        }
        
        ip = ip << 8;                               // store it 
        ip = ip  | val;
        
        ipString += (len + 1);                      // move forward in the string
    }
    
    return ip;
}

