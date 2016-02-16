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

BYTE find_STiNG(void);

void doTest00(void);
void doTest01(void);

BYTE readBuffer [130 * 1024];
BYTE writeBuffer[130 * 1024];

BYTE *rBuf, *wBuf;

int main(void)
{
    initBuffer();

    rBuf = readBuffer;
    wBuf = writeBuffer;
    
    (void) Cconws("STiNG test...\r\n");

    BYTE found = Supexec (find_STiNG);
    if(!found) {
        sleep(3);
        return 0;
    }

    doTest00();
    doTest01();
    
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



