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
BYTE rwFile(BYTE readNotWrite, BYTE *pBfr, WORD sectCnt);
void seekAndRead(void);
void bfrCompare(BYTE *a, BYTE *b, DWORD cnt, DWORD testNo);
void getDriveForTests(void);

void testLoop(void);
BYTE *wBfr, *pWrBfr;
BYTE *rBfr;

#define TESTFILE		"C:\\testfile.bin"

#define Clear_home()    (void) Cconws("\33E")
BYTE rwSectorsNotFiles;

#define BUFSIZE		(128 * 512)
char testFile[128];

int main(int argc, char *argv[])
{
	DWORD i, s, ind;
	BYTE key, val;

	// alloc buffers
    wBfr = (BYTE *) malloc(BUFSIZE);
    rBfr = (BYTE *) malloc(BUFSIZE);

    if(!wBfr || !rBfr) {
        printf("Malloc failed!\n\r");
        return 0;
    }

    // fill write buffer
	ind = 0;
	for(s=0; s<128; s++) {
		for(i=0; i<512; i++) {
			val			= ((BYTE) i) ^ ((BYTE) s);
			wBfr[ind]	= val;
			ind++;
		}
    }
	
	Clear_home();
    printf("RW test.\n\r");
    printf("S - write and read sectors\n\r");
    printf("F - write and read files\n\r");
    printf("E - seek and read file test\n\r");
    printf("Q - quit\n\r");

	rwSectorsNotFiles	= 1;
	
	while(1) {
		key = (BYTE) Cnecin();
		
		if(key >= 97 && key <= 122) {		// to upper case
			key -= 32;
		}
		
		if(key == 'Q') {
			break;
		}
		
		if(key == 'S') {
			Clear_home();
			printf("write and read sectors\n\r");
			
			rwSectorsNotFiles = 1;
			break;
		}
		
		if(key == 'F') {
			getDriveForTests();

			Clear_home();
			printf("write and read files\n\r");
			
			rwSectorsNotFiles = 0;
			break;
		}
		
		if(key == 'E') {
			getDriveForTests();

			Clear_home();
			printf("seek and read file test\n\r");
			break;
		}		
	}
	
	if(key == 'S' || key == 'F') {
		Supexec(testLoop);
		Cnecin();
	}
	
	if(key == 'E') {
		seekAndRead();
		Cnecin();
	}
		
    // release buffers
    free(wBfr);
    free(rBfr);

    printf("\n\rDone.\n\r");
	return 0;
}

void getDriveForTests(void)
{
	Clear_home();

	printf("Enter DRIVE LETTER on which the tests will be done: \n\r");

	BYTE drive, key;
	while(1) {
		key = (BYTE) Cnecin();
		
		if(key >= 97 && key <= 122) {		// to upper case
			key -= 32;
		}

		if(key >= 'A' && key <= 'P') {
			drive = key;
			break;
		}
	}
	
	strcpy(testFile, TESTFILE);
	testFile[0] = drive;
}

void seekAndRead(void)
{
	int i;
	int16_t handle;
	DWORD offset, length, maxLen;
	int32_t res;
	
	res = rwFile(0, wBfr, 128);				// write 128 sectors

	handle = Fopen(testFile, 0);

	if(handle < 0) {
		printf("Failed to open file %s\n\r", testFile);
		return;
	}
	
	for(i=0; i<1000; i++) {
        if((i % 10) == 0) {
            printf("\n\r% 4d ", i);
        } else {
			Cconout('.');
		}
		
		offset	= Random() % BUFSIZE;
		
		maxLen	= BUFSIZE - offset;
		length	= Random() % maxLen;
		
		Fseek(offset, handle, 0);
		
		res = Fread(handle, length, rBfr);
		
		if(res < 0) {
			printf("Fread() reported error %d\n\r", res);
			continue;
		}
		
		if(res < length) {
			printf("Fread() didn't read enough (%d < %d)\n\r", res, length);
			continue;
		}
		
		bfrCompare(wBfr + offset, rBfr, length, i);
	}
	
	Fclose(handle);
}

void testLoop(void)
{
    int i, sectStart, sectCnt;
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
        if((sectCnt & 1) != 0) { 			// odd address?
            pWrBfr += 1;                    // make even address
        }

        cmd[1] = (BYTE) (sectStart >> 16);
        cmd[2] = (BYTE) (sectStart >>  8);
        cmd[3] = (BYTE) (sectStart);

        cmd[4] = (BYTE) sectCnt;
        cmd[5] = 0;

        cmd[0] = 0x0a;

		if(rwSectorsNotFiles == 0) {								// test on files
			res = rwFile(0, pWrBfr, sectCnt);
		} else {													// test on sectors
			res = acsi_cmd(0, cmd, 6, pWrBfr, sectCnt);           	// write data
		}
			
        if(res != 0) {
            printf("\n\rWrite operation failed at test %d\n\r", i);
            continue;
        }

        cmd[0] = 0x08;
		
		if(rwSectorsNotFiles == 0) {								// test on files
			res = rwFile(1, pRdBfr, sectCnt);
		} else {													// test on sectors
			res = acsi_cmd(1, cmd, 6, pRdBfr, sectCnt);           	// read data		
		}
			
        if(res != 0) {
            printf("\n\rRead operation failed at test %d\n\r", i);
            continue;
        }

		DWORD cnt = sectCnt * 512;
		bfrCompare(pWrBfr, pRdBfr, cnt, i);
    }
}

void bfrCompare(BYTE *a, BYTE *b, DWORD cnt, DWORD testNo)
{
	DWORD j;

    for(j=0; j<cnt; j++) {
        if(a[j] != b[j]) {
            printf("\n\rData mismatch at test %d, index %d\n\r", testNo, j);
            printf("should be: %02x %02x %02x %02x\n\r", a[j + 0], a[j + 1], a[j + 2], a[j + 3]);
            printf("really is: %02x %02x %02x %02x\n\r", b[j + 0], b[j + 1], b[j + 2], b[j + 3]);
			(void) Cnecin();
            break;
        }
    }
}

BYTE rwFile(BYTE readNotWrite, BYTE *pBfr, WORD sectCnt)
{
	int16_t handle;
	BYTE mode;
	DWORD res;
	char *opStr;
	
	if(readNotWrite) {
		mode = 0;
		opStr = "READ";
		
		handle = Fopen(testFile, mode);
	} else {
		Fdelete(testFile);
	
		mode = 1;
		opStr = "WRITE";

		handle = Fcreate(testFile, 0);
	}
	
	if(handle < 0) {
		printf("Fopen / Fcreate: to %s file %s (err: %d)\n\r", opStr, testFile, handle);
		return 0xff;
	}

	DWORD length = ((DWORD) sectCnt) << 9;
	if(readNotWrite) {
		res = Fread(handle, length, pBfr);
	} else {
		res = Fwrite(handle, length, pBfr);
	}
	
	if(res != length) {
		printf("Fread / Fwrite: Failed to %s on file\n\r", opStr);
		res = 0xff;
	} else {
		res = 0;
	}
	
	Fclose(handle);
	return res;
}

