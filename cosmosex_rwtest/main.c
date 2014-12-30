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

#include "acsi.h"

#define BYTE    unsigned char
#define WORD  	uint16_t
#define DWORD 	uint32_t

BYTE  acsiRW(BYTE readNotWrite, DWORD sectStart, BYTE sectCnt, BYTE *bfr);
DWORD getDriveCapacity(void);
void showRWerror(BYTE rwSectorsNotFiles, int i, BYTE readNotWrite, DWORD sectStart, WORD sectCnt, BYTE res);

BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
BYTE rwFile(BYTE readNotWrite, BYTE *pBfr, WORD sectCnt);
void seekAndRead(void);
void bfrCompare(BYTE *a, BYTE *b, DWORD cnt, DWORD testNo);
void getDriveForTests(void);
void showInt(int value, int length);
void showHexByte(int val);

void testLoop(void);
BYTE *wBfrOrig, *rBfrOrig;
BYTE *wBfr,     *rBfr;

void showCmdLog(void);

#define TESTFILE		"C:\\testfile.bin"

#define Clear_home()    (void) Cconws("\33E")
BYTE rwSectorsNotFiles;

#define BUFSIZE		(MAXSECTORS * 512)
char testFile[128];

extern BYTE acsiBufferClear;        // should clear FIFO buffer on each acsi_cmd() call? 

int main(int argc, char *argv[])
{
	DWORD i, s, ind;
	BYTE key, val;

    acsiBufferClear = 1;            // DO clear FIFO buffer on each acsi_cmd() call
    
	// alloc buffers
    wBfrOrig = (BYTE *) Malloc(BUFSIZE + 2);
    rBfrOrig = (BYTE *) Malloc(BUFSIZE + 2);

    if(!wBfrOrig || !rBfrOrig) {
        (void) Cconws("Malloc failed!\n\r");
        return 0;
    }

    wBfr = (BYTE *) ((((DWORD) wBfrOrig) + 2) & 0xfffffffe);
    rBfr = (BYTE *) ((((DWORD) rBfrOrig) + 2) & 0xfffffffe);
    
    // fill write buffer
	ind = 0;
	for(s=0; s<MAXSECTORS; s++) {
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
    Mfree(wBfrOrig);
    Mfree(rBfrOrig);

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
    BYTE res;
    
    DWORD capacity;

    if(rwSectorsNotFiles) {                             // sector level test? get drive capacity using SCSI command
        capacity = getDriveCapacity();
    
        if(capacity == 0) {
            return;
        }
    } else {                                            // file level test? just fake some capacity (we will be reading just 128 kB at once)
        capacity = 15 * 1024 * 1024;
    }
    
    for(i=0; i<1000; i++) {
        //----------
        // show some progress to user 
        if((i % 10) == 0) {
            (void) Cconws("\n\r");
			showInt(i, 4);
			(void) Cconws(" ");
        } else {
			Cconout('.');
		}

        //----------
        // make up operation params
        sectCnt     = (Random() % MAXSECTORS) + 1;      // count of sectors to read and write
        sectStart   = (Random() % capacity  );          // starting sector - 0 .. capacity
        
        if(sectStart + sectCnt >= capacity) {           // would we go out of capacity range? 
            sectStart = capacity - sectCnt;             // read the last sectors 
        }

        //----------
        // write data
		if(rwSectorsNotFiles == 0) {								// test on files
			res = rwFile(0, wBfr, sectCnt);
		} else {													// test on sectors
            res = acsiRW(ACSI_WRITE, sectStart, sectCnt, wBfr);
		}
			
        if(res != 0) {                                              // on WRITE error
            showRWerror(rwSectorsNotFiles, i, ACSI_WRITE, sectStart, sectCnt, res);
            continue;
        }

        //----------
        // read data
        DWORD byteCount = sectCnt * 512;
        memset(rBfr, 0, byteCount);                                 // clean the buffer before reading
        
		if(rwSectorsNotFiles == 0) {								// test on files
			res = rwFile(1, rBfr, sectCnt);
		} else {													// test on sectors
            res = acsiRW(ACSI_READ, sectStart, sectCnt, rBfr);
		}
			
        if(res != 0) {                                              // on READ error
            showRWerror(rwSectorsNotFiles, i, ACSI_READ, sectStart, sectCnt, res);
            continue;
        }

        //----------
        // verify data
		DWORD cnt = sectCnt * 512;
		bfrCompare(wBfr, rBfr, cnt, i);                             // check data validity, possibly display error
    }
}

void showRWerror(BYTE rwSectorsNotFiles, int i, BYTE readNotWrite, DWORD sectStart, WORD sectCnt, BYTE res)
{
    if(readNotWrite) {
        (void) Cconws("\n\rREAD  ");
    } else {
        (void) Cconws("\n\rWRITE ");
    }

    (void) Cconws("operation failed at test ");
    showInt(i, 4);
    
    if(rwSectorsNotFiles) {                                 // for sector level test also write starting sector
        (void) Cconws("\n\rsectStart: ");
        showHexByte(sectStart >> 24);
        showHexByte(sectStart >> 16);
        showHexByte(sectStart >>  8);
        showHexByte(sectStart      );
    }
    
    (void) Cconws("\n\rsectCnt  : ");
    showHexByte(sectCnt >> 8);
    showHexByte(sectCnt     );
    
    (void) Cconws("\n\rres      : ");
    showHexByte(res);
    (void) Cconws("\n\r");
    
    if(rwSectorsNotFiles) {
        showCmdLog();
    }
    
    Cnecin();
}

DWORD getDriveCapacity(void)
{
    BYTE cmd[11];
    BYTE res;

    acsiBufferClear = 0;                    // DON'T clear FIFO buffer on each acsi_cmd() call
    
    memset(cmd, 0, 11);
    cmd[0] = 0x3f;                          // ICD cmd marker
    cmd[1] = 0x25;                          // SCSI_C_READ_CAPACITY                    
    
    res = acsi_cmd(ACSI_READ, cmd, 11, rBfr, 1);
    
    if(res != 0) {
        acsiBufferClear = 1;                // DO clear FIFO buffer on each acsi_cmd() call

        (void) Cconws("\n\rFailed to get capacity of ACSI device 1. (1st GET CAPACITY failed)\n\r");
        Cnecin();
        return 0;
    }

    res = acsi_cmd(ACSI_READ, cmd, 11, rBfr, 1);
    acsiBufferClear = 1;                // DO clear FIFO buffer on each acsi_cmd() call
    
    if(res != 0) {
        (void) Cconws("\n\rFailed to get capacity of ACSI device 1. (2nd GET CAPACITY failed)\n\r");
        Cnecin();
        return 0;
    }

    (void) Cconws("Capacity: ");
    showHexByte(rBfr[0]);
    showHexByte(rBfr[1]);
    showHexByte(rBfr[2]);
    showHexByte(rBfr[3]);
    (void) Cconws("\n\r");
    
    DWORD capacity;
    capacity  = rBfr[0];
    capacity  = capacity << 8;
    capacity |= rBfr[1];
    capacity  = capacity << 8;
    capacity |= rBfr[2];
    capacity  = capacity << 8;
    capacity |= rBfr[3];
    
    if(capacity == 0) {
        (void) Cconws("\n\rGET CAPACITY returned 0, can't continue.\n\r");
        Cnecin();
    }    
    
    return capacity;
}

BYTE acsiRW(BYTE readNotWrite, DWORD sectStart, BYTE sectCnt, BYTE *bfr)
{
    BYTE cmd[11];
    BYTE cmdLength;

    if(sectStart <= 0x1fffff) {             // less than 1 GB address?
        cmdLength = 6;
    
        if(readNotWrite == ACSI_READ) {     // on read
            cmd[0] = 0x28;                  // SCSI READ 6  + ACSI ID 1
        } else {                            // on write
            cmd[0] = 0x2a;                  // SCSI WRITE 6 + ACSI ID 1
        }
    
        cmd[1] = (BYTE) (sectStart >> 16);  // address
        cmd[2] = (BYTE) (sectStart >>  8);
        cmd[3] = (BYTE) (sectStart);

        cmd[4] = (BYTE) sectCnt;            // length
        cmd[5] = 0;                         // control
    } else {                                // more than 1 GB address?
        cmdLength = 11;
        
        cmd[0] = 0x3f;                      // ACSI ID 1 + ICD command flag (0x1f)
        
        if(readNotWrite == ACSI_READ) {     // on read
            cmd[1] = 0x28;                  // SCSI READ 10
        } else {                            // on write
            cmd[1] = 0x2a;                  // SCSI WRITE 10
        }
        
        cmd[2] = 0;
    
        cmd[3] = (BYTE) (sectStart >> 24);  // address
        cmd[4] = (BYTE) (sectStart >> 16);
        cmd[5] = (BYTE) (sectStart >>  8);
        cmd[6] = (BYTE) (sectStart);

        cmd[7] = 0;                         // group number

        cmd[8] = 0;                         // length hi
        cmd[9] = (BYTE) sectCnt;            // length lo
        
        cmd[10] = 0;                        // control
    }

    BYTE res = acsi_cmd(readNotWrite, cmd, cmdLength, bfr, sectCnt);
    return res;
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
	
	if(readNotWrite) {
		mode = 0;
		handle = Fopen(testFile, mode);
	} else {
		Fdelete(testFile);
		mode = 1;
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


