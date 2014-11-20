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
void showInt(int value, int length);
void showHexByte(int val);

void testLoop(void);
BYTE *wBfr, *pWrBfr;
BYTE *rBfr;

#define TESTFILE		"C:\\testfile.bin"

#define Clear_home()    (void) Cconws("\33E")
BYTE rwSectorsNotFiles;

#define BUFSIZE		(128 * 512)
char testFile[128];

BYTE wrBfr[BUFSIZE];
BYTE rdBfr[BUFSIZE];

int main(int argc, char *argv[])
{
	DWORD i, s, ind;
	BYTE key, val;

	// alloc buffers
//    wBfr = (BYTE *) malloc(BUFSIZE);
//    rBfr = (BYTE *) malloc(BUFSIZE);
	wBfr = wrBfr;
	rBfr = rdBfr;	

    if(!wBfr || !rBfr) {
        (void) Cconws("Malloc failed!\n\r");
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
    (void) Cconws("RW test.\n\r");
    (void) Cconws("S - write and read sectors\n\r");
    (void) Cconws("F - write and read files\n\r");
    (void) Cconws("E - seek and read file test\n\r");
    (void) Cconws("Q - quit\n\r");

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
			(void) Cconws("write and read sectors\n\r");
			
			rwSectorsNotFiles = 1;
            Supexec(testLoop);
			
			(void) Cconws("\n\rDone.\n\r");
		}
		
		if(key == 'F') {
			getDriveForTests();

			Clear_home();
			(void) Cconws("write and read files\n\r");
			
			rwSectorsNotFiles = 0;
            Supexec(testLoop);

			(void) Cconws("\n\rDone.\n\r");
		}
		
		if(key == 'E') {
			getDriveForTests();

			Clear_home();
			(void) Cconws("seek and read file test\n\r");
            seekAndRead();

			(void) Cconws("\n\rDone.\n\r");
		}		
	}
		
    // release buffers
//    free(wBfr);
//    free(rBfr);

	return 0;
}

void getDriveForTests(void)
{
	Clear_home();

	(void) Cconws("Enter DRIVE LETTER on which the tests will be done: \n\r");

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
		(void) Cconws("Failed to open file ");
		(void) Cconws(testFile);
		(void) Cconws("\n\r");
		return;
	}
	
	for(i=0; i<100; i++) {
        if((i % 10) == 0) {
            (void) Cconws("\n\r");
			showInt(i, 4);
        } else {
			Cconout('.');
		}
		
		offset	= Random() % BUFSIZE;
		
		maxLen	= BUFSIZE - offset;
		length	= Random() % maxLen;
		
		Fseek(offset, handle, 0);
		
		res = Fread(handle, length, rBfr);
		
		if(res < 0) {
			(void) Cconws("Fread() reported error ");
			showHexByte(res);
			(void) Cconws("\n\r");
			continue;
		}
		
		if(res < length) {
			(void) Cconws("Fread() didn't read enough (");
			showInt(res, 5);
			(void) Cconws(" < ");
			showInt(length, 5);
			(void) Cconws("\n\r");
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
            (void) Cconws("\n\r");
			showInt(i, 4);
			(void) Cconws(" ");
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
            (void) Cconws("\n\rWrite operation failed at test ");
			showInt(i, 4);
			(void) Cconws("\n\r");
            continue;
        }

        cmd[0] = 0x08;
		
		if(rwSectorsNotFiles == 0) {								// test on files
			res = rwFile(1, pRdBfr, sectCnt);
		} else {													// test on sectors
			res = acsi_cmd(1, cmd, 6, pRdBfr, sectCnt);           	// read data		
		}
			
        if(res != 0) {
            (void) Cconws("\n\rRead operation failed at test ");
			showInt(i, 4);
			(void) Cconws("\n\r");
            continue;
        }

		DWORD cnt = sectCnt * 512;
		bfrCompare(pWrBfr, pRdBfr, cnt, i);
    }
}

void bfrCompare(BYTE *a, BYTE *b, DWORD cnt, DWORD testNo)
{
	DWORD j, k;

    for(j=0; j<cnt; j++) {
        if(a[j] != b[j]) {
            (void) Cconws("\n\rData mismatch at test ");
			showInt(testNo, -1);
			(void) Cconws(", index ");
			showInt(j, -1);
			(void) Cconws("\n\r");
            (void) Cconws("should be: ");
			
			for(k=0; k<4; k++) {
				showHexByte(a[j + k]);
				(void) Cconws(" ");
			}
			(void) Cconws("\n\r");

            (void) Cconws("really is: ");
			for(k=0; k<4; k++) {
				showHexByte(b[j + k]);
				(void) Cconws(" ");
			}
			(void) Cconws("\n\r");

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
		(void) Cconws("\n\rFopen / Fcreate failed for file: ");
		(void) Cconws(testFile);
		(void) Cconws("\n\r");
		return 0xff;
	}

	DWORD length = ((DWORD) sectCnt) << 9;
	if(readNotWrite) {
		res = Fread(handle, length, pBfr);
	} else {
		res = Fwrite(handle, length, pBfr);
	}
	
	if(res != length) {
		(void) Cconws("\n\rFread / Fwrite failed for file: ");
		(void) Cconws(testFile);
		(void) Cconws("\n\r");
		res = 0xff;
	} else {
		res = 0;
	}
	
	Fclose(handle);
	return res;
}

void showHexByte(int val)
{
    int hi, lo;
    char table[16] = {"0123456789ABCDEF"};
    hi = (val >> 4) & 0x0f;;
    lo = (val     ) & 0x0f;

    (void) Cconout( table[hi] );
    (void) Cconout( table[lo] );
}

void showInt(int value, int length)
{
    char tmp[10];
    memset(tmp, 0, 10);

    if(length == -1) {                      // determine length?
        int i, div = 10;

        for(i=1; i<6; i++) {                // try from 10 to 1000000
            if((value / div) == 0) {        // after division the result is zero? we got the length
                length = i;
                break;
            }

            div = div * 10;                 // increase the divisor by 10
        }

        if(length == -1) {                  // length undetermined? use length 6
            length = 6;
        }
    }

    int i;
    for(i=0; i<length; i++) {               // go through the int lenght and get the digits
        int val, mod;

        val = value / 10;
        mod = value % 10;

        tmp[length - 1 - i] = mod + 48;     // store the current digit

        value = val;
    }

    (void) Cconws(tmp);                     // write it out
} 
