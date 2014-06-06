#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <gem.h>
#include <malloc.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BYTE    unsigned char
#define WORD  	uint16_t
#define DWORD 	uint32_t

BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

void testLoop(void);
BYTE *wBfr, *pWrBfr;
BYTE *rBfr;

int main(int argc, char *argv[])
{
	int i;

    printf("RW test.\n\r");

    // alloc buffers
    wBfr = (BYTE *) malloc(128 * 512);
    rBfr = (BYTE *) malloc(128 * 512);

    if(!wBfr || !rBfr) {
        printf("Malloc failed!\n\r");
        return 0;
    }

    // fill write buffer
    for(i=0; i<(128 * 512); i++) {
        wBfr[i] = (BYTE) i;
    }

	Supexec(testLoop);

    // release buffers
    free(wBfr);
    free(rBfr);

    printf("\n\rDone.\n\r");
	return 0;
}

void testLoop(void)
{
    int i, sectStart, sectCnt, j;
    BYTE cmd[6], res;
    
    BYTE *pRdBfr;
    DWORD dRdBfr = (DWORD) (rBfr + 2);      // get original RAM address + 2
    dRdBfr = dRdBfr & 0xfffffffe;           // get even version of that... 
    pRdBfr = (BYTE *) dRdBfr;               // convert it to pointer

    for(i=0; i<1000; i++) {
        if((i % 10) == 0) {
            printf("\n\r% 4d ", i);
        } else {
			Cconout('.');
		}

        sectStart   = Random() % 10000;       // starting sector - 0 .. 10'000
        sectCnt     = (Random() % 124) + 1;   // count of sectors to read and write

        pWrBfr = wBfr + sectCnt;            // calculate the starting pointer
        if((sectStart & 0xfffffffe) != 0) { // odd address?
            pWrBfr += 1;                    // make even address
        }

        cmd[1] = (BYTE) (sectStart >> 16);
        cmd[2] = (BYTE) (sectStart >>  8);
        cmd[3] = (BYTE) (sectStart);

        cmd[4] = (BYTE) sectCnt;
        cmd[5] = 0;

        cmd[0] = 0x0a;
        res = acsi_cmd(0, cmd, 6, pWrBfr, sectCnt);           // write data

        if(res != 0) {
            printf("\n\rWrite command failed at test %d\n\r", i);
            continue;
        }

        cmd[0] = 0x08;
        res = acsi_cmd(1, cmd, 6, pRdBfr, sectCnt);           // read data

        if(res != 0) {
            printf("\n\rRead command failed at test %d\n\r", i);
            continue;
        }

        for(j=0; j<(sectCnt * 512); j++) {
            if(pWrBfr[j] != pRdBfr[j]) {
                printf("\n\rData mismatch at test %d, index %d\n\r", i, j);
                printf("should be: %02x %02x %02x %02x\n\r", pWrBfr[j + 0], pWrBfr[j + 1], pWrBfr[j + 2], pWrBfr[j + 3]);
                printf("really is: %02x %02x %02x %02x\n\r", pRdBfr[j + 0], pRdBfr[j + 1], pRdBfr[j + 2], pRdBfr[j + 3]);
                break;
            }
        }
    }
}