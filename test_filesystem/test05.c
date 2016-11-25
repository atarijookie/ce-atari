#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdio.h>
#include "stdlib.h"
#include "out.h"

extern int  drive;
extern WORD tosVersion;

static void test050x(void);
static void test051x(WORD testCaseOffset, BYTE *rdBfr);    // tests 051x, 052x, 053x
static void test054x(void);                                // tests 054x, 055x, 056x
static void test057x(void);
static void test058x(void);
static void test05ax(void);
static void test05bx(void);
static void test05dx(void);
static void test05ex(void);

BYTE filenameExists(char *filename);
void deleteIfExists(char *fname);

int  openFile(WORD xorVal);
BYTE createFile(DWORD size, WORD xorVal);
void deleteFile(WORD xorVal);
void fillBuffer(DWORD size, WORD xorVal, BYTE *bfr);

BYTE testFreadByBlockSize(DWORD size, WORD xorVal, DWORD blockSize, BYTE *rdBfr);
BYTE testFreadByBlockSizeArray(DWORD size, WORD xorVal, DWORD *blockSizeArray, WORD blockSizeCount, BYTE *rdBfr);
BYTE verifySeek(DWORD offset, int handle);

BYTE testFwriteByBlockSizeArray(DWORD size, DWORD *blockSizeArray, WORD blockSizeCount, BYTE *wrBfr);
void testFileWrite(WORD tcOffs, BYTE *wrBfr);

BYTE *buf1, *buf2;
#define TEST051XFILESIZE    (200 * 1024)
#define BUFFERSIZE          (220 * 1024)

void test05(WORD whichTests)
{
    out_s("Fcreate, Fopen, Fclose, Fread, Fwrite, Fseek");
    
    Dsetdrv(drive);
    Dsetpath("\\");

    buf1 = (BYTE *) Malloc(BUFFERSIZE);
    buf2 = (BYTE *) Malloc(BUFFERSIZE);
    if(!buf1 || !buf2) {
        out_s("Failed to allocate buffers, terminating.");
        
        if(buf1) {
            Mfree(buf1);
        }
        
        if(buf2) {
            Mfree(buf2);
        }
        
        return;
    }
    
    out_s("Creating test file...");
    int res = createFile(TEST051XFILESIZE, 0xABCD);
    if(!res) {
        out_tr_b(0x0510, "Failed to create file!", 0);
        return;
    }

    if(whichTests & 0x01) {         // O
        test050x();                 // Fopen, Fcreate, Fclose
    }

    if(whichTests & 0x02) {         // R
        test051x(0, buf2);          // read by different block sizes into ST RAM - tests 051x, 052x, 053x
        test054x();                 // read by different block sizes into TT RAM - tests 054x, 055x, 056x 
        test057x();                 // Fread - various cases
    }
     
    if(whichTests & 0x04) {         // S
        test058x();                 // Fseek + read
    }
    
    if(whichTests & 0x08) {         // X
        test05ax();                 // Fread - 10 files open and read
    }
     
    if(whichTests & 0x10) {         // W
        test05bx();                 // Fwrite - tests 05Bx, 05Cx
    }
    
    if(whichTests & 0x20) {         // V
        test05dx();                 // Fwrite - various cases
    }

    if(whichTests & 0x40) {         // T
        test05ex();                 // Fseek + write
    }
    
    deleteFile(0xABCD);
    
    Mfree(buf1);
    Mfree(buf2);
    
    out_s("");
}

#define TEST05DCFILE    "DERP"
void test05dx(void)
{
    WORD res;
    BYTE ok;
    int f;
    
    char bfr[27] = {"Herpy derpy this is a test"};
    
    res = Fwrite(100, 27, bfr);
    (res == 0xffdb) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x05d1, "Fwrite - to invalid handle (100)", ok, res);

    Fdelete(TEST05DCFILE);
    f   = Fcreate(TEST05DCFILE, 0);
    res = Fwrite(f, 27, bfr);
    Fclose(f);
    (res == 27) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x05d2, "Fwrite - to newly created file", ok, res);
    
    f = Fopen(TEST05DCFILE, 1);
    res = Fwrite(f, 27, bfr);
    Fclose(f);
    (res == 27) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x05d3, "Fwrite - to file opened as WRITE-ONLY", ok, res);
    
    f = Fopen(TEST05DCFILE, 2);
    res = Fwrite(f, 27, bfr);
    Fclose(f);
    (res == 27) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x05d4, "Fwrite - to file opened as READ-WRITE", ok, res);
    
    f = Fopen(TEST05DCFILE, 3);
    res = Fwrite(f, 27, bfr);
    Fclose(f);
    (res == 27) ? (ok = 1) : (ok = 0);                  // TOS bug? Fwrite returns that the data was written, even though the file should be READ-ONLY  
    out_tr_bw(0x05d5, "Fwrite - to file opened as READ-ONLY", ok, res);

    res = Fwrite(f, 27, bfr);
    Fclose(f);
    (res == 0xffdb) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x05d6, "Fwrite - to file which was closed", ok, res);
    
    Fdelete(TEST05DCFILE);
}

void test05bx(void)
{
    out_s("Tests 05Bx - Fwrite() from ST RAM");
    testFileWrite(0x0000, buf1);        // do the write tests in ST RAM
    
    //-------------------------
    if((tosVersion >> 8) < 3) {         // if TOS version is less than 3 (it's and ST, not TT or Falcon)
        out_s("Tests 05Cx will be skipped as they are for TT RAM.");
        return;
    } else {
        out_s("Tests 05Cx - Fwrite() from TT RAM");
    }
    
    BYTE *bufTT = (BYTE *) Mxalloc(BUFFERSIZE, 1);
    
    if(bufTT == 0) {
        out_s("Tests 05Cx will be skipped - failed to allocate TT RAM.");
        return;
    }

    memcpy(buf1, bufTT, BUFFERSIZE);    // copy the generated data from ST RAM to TT RAM
    testFileWrite(0x0010, bufTT);
    Mfree(bufTT);    
}

void testFileWrite(WORD tcOffs, BYTE *wrBfr)
{
    DWORD sz;
    BYTE ok;
    WORD res;

    //-----------------
    // test file write - single block size
    sz = 127;
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, &sz, 1, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05b1 + tcOffs, "Fwrite - block size:    127", ok);
    
    sz = 511;
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, &sz, 1, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05b2 + tcOffs, "Fwrite - block size:    511", ok);
    
    sz = 512;
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, &sz, 1, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05b3 + tcOffs, "Fwrite - block size:    512", ok);
    
    sz = 15000;
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, &sz, 1, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05b4 + tcOffs, "Fwrite - block size:  15000", ok);
    
    sz = 25000;
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, &sz, 1, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05b5 + tcOffs, "Fwrite - block size:  25000", ok);
    
    //----------------
    // test file write - different block sizes
    
    DWORD arr1[2] = {5, 127};
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, arr1, 2, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05b6 + tcOffs, "Fwrite - var. bl.: 5, 127", ok);

    DWORD arr2[3] = {127, 256, 512};
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, arr2, 3, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05b7 + tcOffs, "Fwrite - var. bl.: 127, 256, 512", ok);

    DWORD arr3[3] = {250, 512, 513};
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, arr3, 3, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05b8 + tcOffs, "Fwrite - var. bl.: 250, 512, 513", ok);

    DWORD arr4[4] = {250, 550, 10, 1023};
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, arr4, 4, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05b9 + tcOffs, "Fwrite - var. bl.: 250, 550, 10, 1023", ok);

    DWORD arr5[4] = {1023, 2048, 5000, 100};
    res = testFwriteByBlockSizeArray(TEST051XFILESIZE, arr5, 4, wrBfr);
    (res == 1) ? (ok = 1) : (ok = 0);
    out_tr_b(0x05ba + tcOffs, "Fwrite - var. bl.: 1023, 2048, 5000, 100", ok);
}

void test05ax(void)
{
    int res;
    int i, f[10];
    int rdCnt[10];

    out_s("Testing reading of 10 open files at the same time...");
    
    for(i=0; i<10; i++) {               // open files
        f[i]        = openFile(0xABCD);
        rdCnt[i]    = 0;
    }
    
    int order[10] = {5,1,7,4,8,0,2,9,6,3};
    int size[8] = {10,520,2500,2,15000,20000,480,100};
    int oInd = 0, sInd = 0;
    
    int idx, sz, allOk, found;
    int dataOk = 1;
    
    DWORD tLast = 0, tNow, tDiff;
    
    while(1) {
        //------------------
        // show progress every second
        tNow    = getTicks();
        tDiff   = tNow - tLast;
        
        if(tDiff > 200) {           // if at least second passed since last output, show something
            tLast = tNow;
            Cconout('*');
        }
    
        //------------------
        // first check if at least one open file needs to be read... If everything is fully read, quit
        allOk = 1;
        
        for(i=0; i<10; i++) {
            if(f[i] <= 0) {                                     // file not open? skip it
                continue;
            }
            
            if(rdCnt[i] < TEST051XFILESIZE) {
                allOk = 0;
                break;
            }
        }
        if(allOk) {
            break;
        }
    
        //------------------
        // find a file which needs to be read next
        found = 0;
        for(i=0; i<10; i++) {                                   // go through all the files
            idx = order[oInd];                                  // get the index of file

            oInd++;
            if(oInd >= 10) {                                    // update the order index
                oInd = 0;
            }
            
            if(rdCnt[idx] >= TEST051XFILESIZE || f[idx] <= 0) { // if reading of this file is done, skip it
                continue;
            }
            
            found = 1;
            break;
        }
    
        if(!found) {                                            // if didn't find the file, try the whole loop once again (and possibly quit)
            continue;
        }
    
        //------------------
        // do the reading of that file
        sz = size[sInd];                                        // get size of read whe should do
        sInd++;
        if(sInd >= 8) {
            sInd = 0;
        }
    
        int offset = rdCnt[idx];
        
        memset(buf2 + offset, 0, sz);                           // clear the RAM to make sure the data was really read
        res = Fread(f[idx], sz, buf2 + offset);                 // try to read from the file
        
        int rCnt = 0;                                           // how many bytes was really read? (e.g. when end of file is hit, you might read less than what you wanted)
        if(res > 0) {                                           // reading success?
            rdCnt[idx] += res;
            rCnt = res;
        }
        
        res = memcmp(buf1 + offset, buf2 + offset, rCnt);       // compare data
        if(res != 0) {                                          // some difference in data?
            dataOk = 0;
        }
    }
    
    allOk = 1;
    for(i=0; i<10; i++) {                                       // close files
        if(f[i] > 0) {
            Fclose(f[i]);
        }
        
        if(rdCnt[i] < TEST051XFILESIZE) {                       // check if the file is fully read
            allOk = 0;
            break;
        }
    }
    
    (void) Cconws("\r\n");
    
    out_tr_b(0x05A1, "Fread - 10 files reading - total length", allOk);
    out_tr_b(0x05A2, "Fread - 10 files reading - data validity",dataOk);
}

void test058x(void)
{
    int f, res1, res2, ok;

    f = openFile(0xABCD);
    
    if(f < 0) {
        out_s("058x - Failed to open file");
        return;
    }
    
    #define SEEK_SET    0
    #define SEEK_CUR    1
    #define SEEK_END    2
    
    //---------
    // test SEEK_SET
    res1 = Fseek(0, f, SEEK_SET);
    res2 = verifySeek(0, f);
    (res1 == 0 && res2 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0581, "Fseek - SEEK_SET to start while at start", ok, res1);
    
    res1 = Fseek(TEST051XFILESIZE/2, f, SEEK_SET);
    res2 = verifySeek(TEST051XFILESIZE/2, f);
    (res1 == (TEST051XFILESIZE/2) && res2 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0582, "Fseek - SEEK_SET to half", ok, res1);

    res1 = Fseek(TEST051XFILESIZE, f, SEEK_SET);
    res2 = Fread(f, 16, buf2);
    (res1 == TEST051XFILESIZE && res2 == 0) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0583, "Fseek - SEEK_SET to end", ok, res1);

    res1 = Fseek(0, f, SEEK_SET);
    res2 = verifySeek(0, f);
    (res1 == 0 && res2 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0584, "Fseek - SEEK_SET back to start", ok, res1);
    
    //---------
    // test SEEK_CUR
    res1 = Fseek(TEST051XFILESIZE/2, f, SEEK_CUR);
    res2 = verifySeek(TEST051XFILESIZE/2, f);
    (res1 == (TEST051XFILESIZE/2) && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0585, "Fseek - SEEK_CUR to half  (+half)", ok, res1);

    res1 = Fseek(TEST051XFILESIZE/2, f, SEEK_CUR);
    res2 = Fread(f, 16, buf2);
    (res1 == TEST051XFILESIZE && res2 == 0) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0586, "Fseek - SEEK_CUR to end   (+half)", ok, res1);

    res1 = Fseek(-(TEST051XFILESIZE/2), f, SEEK_CUR);
    res2 = verifySeek(TEST051XFILESIZE/2, f);
    (res1 == (TEST051XFILESIZE/2) && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0587, "Fseek - SEEK_CUR to half  (-half)", ok, res1);
    
    res1 = Fseek(-(TEST051XFILESIZE/2), f, SEEK_CUR);
    res2 = verifySeek(0, f);
    (res1 == 0 && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0588, "Fseek - SEEK_CUR to start (-half)", ok, res1);
    
    //---------
    // test SEEK_END -- offset is a NEGATIVE number
    res1 = Fseek(-TEST051XFILESIZE, f, SEEK_END);
    res2 = verifySeek(0, f);
    (res1 == 0 && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0589, "Fseek - SEEK_END to start", ok, res1);

    res1 = Fseek(-(TEST051XFILESIZE/2), f, SEEK_END);
    res2 = verifySeek(TEST051XFILESIZE/2, f);
    (res1 == (TEST051XFILESIZE/2) && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0590, "Fseek - SEEK_END to half", ok, res1);
    
    res1 = Fseek(0, f, SEEK_END);
    res2 = Fread(f, 16, buf2);
    (res1 == TEST051XFILESIZE && res2 == 0) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0591, "Fseek - SEEK_END to end", ok, res1);
    
    Fclose(f);    
}

void test05ex(void)
{
	//   32 byte line           01234567890123456789012345678901
	static const char line[] = "This is a Line of the file !!!\r\n";
	char buffer[32];
	char bufferhex[16];
	int i;
	BYTE ok;
	WORD f = 0;
	DWORD written;
	DWORD offset = 0;		// theorical offset in file
	DWORD realoffset = 0;	// offset as returned by Fseek

	Fdelete("TESTB00F");

	f = Fcreate("TESTB00F", 0);
	ok = (f > 0) ? 1 : 0;
    out_tr_bw(0x05e1, "Fcreate - non-existing file", ok, f);
	for(i = 0; i < 1024; i++) {
		written = Fwrite(f, 32, line);
		if(written != 32) {
    		out_tr_bd(0x05e1, "Fwrite - line ", 0, written);
			goto endtest;
		}
		offset += written;
		realoffset = Fseek(0, f, SEEK_CUR);
		if(offset != realoffset) {
			out_tr_bd(0x05e2, "Fseek wrong offset", 0, realoffset);
			goto endtest;
		}
	}

	// seek to middle of the file
	realoffset = Fseek(-32*512, f, SEEK_CUR);
	offset += -32*512;
	ok = (realoffset == offset) ? 1 : 0;
	out_tr_bd(0x05e3, "Fseek to middle of the file ", ok, realoffset);
    dwordToHex(offset, buffer);
	written = Fwrite(f, 8, buffer);
	ok = (written == 8) ? 1 : 0;
	out_tr_bd(0x05e4, "Fwrite in middle of the file ", ok, written);
	offset += written;
	realoffset = Fseek(0, f, SEEK_CUR);
	if(offset != realoffset) {
		out_tr_bd(0x05e5, "Fseek wrong offset", 0, realoffset);
	}
	// seek forward
	realoffset = Fseek(32*3-8, f, SEEK_CUR);
	offset += 32*3-8;
	ok = (realoffset == offset) ? 1 : 0;
	out_tr_bd(0x05e6, "Fseek forward ", ok, realoffset);
    dwordToHex(offset, buffer);
	written = Fwrite(f, 8, buffer);
	ok = (written == 8) ? 1 : 0;
	out_tr_bd(0x05e7, "Fwrite after seek forward ", ok, written);
	offset += written;
	realoffset = Fseek(0, f, SEEK_CUR);
	if(offset != realoffset) {
		out_tr_bd(0x05e8, "Fseek wrong offset", 0, realoffset);
	}
	// seek to end of file
	realoffset = Fseek(-4, f, SEEK_END);
	offset = 32*1024-4;
	ok = (realoffset == offset) ? 1 : 0;
	out_tr_bd(0x05e9, "Fseek to the end of file ", ok, realoffset);
    dwordToHex(offset, buffer);
	written = Fwrite(f, 8, buffer);
	ok = (written == 8) ? 1 : 0;
	out_tr_bd(0x05ea, "Fwrite after seek to end ", ok, written);
	offset += written;
	realoffset = Fseek(0, f, SEEK_CUR);
	if(offset != realoffset) {
		out_tr_bd(0x05eb, "Fseek wrong offset", 0, realoffset);
	}
	// add a partial line
	written = Fwrite(f, 32-4, line+4);
	ok = (written == 32-4) ? 1 : 0;
	out_tr_bd(0x05ec, "Fwrite after end of file ", ok, written);
	offset += written;
	realoffset = Fseek(0, f, SEEK_CUR);
	if(offset != realoffset) {
		out_tr_bd(0x05ed, "Fseek wrong offset", 0, realoffset);
	}
	// SEEK SET
	offset = 32*4;
	realoffset = Fseek(offset, f, SEEK_SET);
	ok = (realoffset == offset) ? 1 : 0;
	out_tr_bd(0x05ee, "Fseek to the start of file ", ok, realoffset);
    dwordToHex(offset, buffer);
	written = Fwrite(f, 8, buffer);
	ok = (written == 8) ? 1 : 0;
	out_tr_bw(0x05ef, "Fwrite ", ok, written);
	offset += written;
	realoffset = Fseek(0, f, SEEK_CUR);
	if(offset != realoffset) {
		out_tr_bd(0x05ef, "Fseek wrong offset", 0, realoffset);
	}
	Fclose(f);
	f = 0;

	f = Fopen("TESTB00F", 0);	//READ ONLY
	offset = 0;
	for(i = 0; i < 1024+1; i++) {
		DWORD read = Fread(f, 32, buffer);
		if(read != 32) {
			out_tr_bd(0x05ef, "file verification : Fread failed", 0, read);
			break;
		}
		switch(offset) {
		case 32*4:
		case 32*512:
		case 32*(512+3):
			dwordToHex(offset, bufferhex);
			ok = (memcmp(buffer, bufferhex, 8) == 0 && memcmp(buffer+8, line+8, 32-8) == 0) ? 1 : 0;
			break;
		case 32*1023:
			ok = (memcmp(buffer, line, 32-4) == 0 && memcmp(buffer+28, "0000", 4) == 0) ? 1 : 0;
			break;
		case 32*1024:
			dwordToHex(offset-4, bufferhex);
			ok = (memcmp(buffer, bufferhex+4, 4) == 0 && memcmp(buffer+4, line+4, 32-4) == 0) ? 1 : 0;
			break;
		default:
			ok = memcmp(buffer, line, 32) == 0 ? 1 : 0;
		}
		if(!ok) {
			out_tr_bd(0x05ef, "file verification : bad content", ok, offset);
		}
		offset += read;
	}

endtest:
	if(f != 0) Fclose(f);
	Fdelete("TESTB00F");
}

void test057x(void)
{
    int f, res1, res2, ok;

    //--------------
    res1 = -1;
    res2 = -1;
    f = openFile(0xABCD);
    if(f) {
        res1 = Fread(f, TEST051XFILESIZE, buf2);
        res2 = Fread(f, TEST051XFILESIZE, buf2);
        Fclose(f);
    }
    (f > 0 && res1 == TEST051XFILESIZE) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0571, "Fread - exact file size", ok, res1);

    (f > 0 && res2 == 0) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0572, "Fread - beyond file size", ok, res2);

    //--------------
    res1 = -1;
    res2 = -1;
    f = openFile(0xABCD);
    if(f) {
        res1 = Fread(f, 0, buf2);
        res2 = Fread(f, TEST051XFILESIZE + 1000, buf2);
        Fclose(f);
    }
    (f > 0 && res1 == 0) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0573, "Fread - zero size of read", ok, res1);

    (f > 0 && res2 == TEST051XFILESIZE) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0574, "Fread - more than what is in the file", ok, res2);

    //--------------
    WORD w = Fread(f, TEST051XFILESIZE, buf2);
    (w == 0xffdb) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0575, "Fread - on invalid handle", ok, w);
}

void test051x(WORD testCaseOffset, BYTE *rdBfr)
{
    int res;

    //---------------------------
    // test by fixed block read size
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 15, rdBfr);
    out_tr_b(0x0511 + testCaseOffset, "Fread - test block size:      15 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 126, rdBfr);
    out_tr_b(0x0512 + testCaseOffset, "Fread - test block size:     126 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 511, rdBfr);
    out_tr_b(0x0513 + testCaseOffset, "Fread - test block size:     511 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 512, rdBfr);
    out_tr_b(0x0514 + testCaseOffset, "Fread - test block size:     512 B", res);
    
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 750, rdBfr);
    out_tr_b(0x0515 + testCaseOffset, "Fread - test block size:     750 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 1023, rdBfr);
    out_tr_b(0x0516 + testCaseOffset, "Fread - test block size:    1023 B", res);
    
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 1024, rdBfr);
    out_tr_b(0x0517 + testCaseOffset, "Fread - test block size:    1024 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 1025, rdBfr);
    out_tr_b(0x0518 + testCaseOffset, "Fread - test block size:    1025 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 63000, rdBfr);
    out_tr_b(0x0519 + testCaseOffset, "Fread - test block size:   63000 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 65536, rdBfr);
    out_tr_b(0x0520 + testCaseOffset, "Fread - test block size:   65536 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 130047, rdBfr);       // ACSI MAX_SECTORS - 1
    out_tr_b(0x0521 + testCaseOffset, "Fread - test block size:  130047 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 130048, rdBfr);       // ACSI MAX_SECTORS
    out_tr_b(0x0522 + testCaseOffset, "Fread - test block size:  130048 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 130049, rdBfr);       // ACSI MAX_SECTORS + 1
    out_tr_b(0x0523 + testCaseOffset, "Fread - test block size:  130049 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 150000, rdBfr);
    out_tr_b(0x0524 + testCaseOffset, "Fread - test block size:  150000 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, TEST051XFILESIZE, rdBfr);
    out_tr_b(0x0525 + testCaseOffset, "Fread - test block size:  204800 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, TEST051XFILESIZE + 1024, rdBfr);
    out_tr_b(0x0526 + testCaseOffset, "Fread - test block size:  205824 B", res);
    
    //---------------------------
    // test by predefined various block sizes
    DWORD arr1[2] = {5, 127};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr1, 2, rdBfr);
    out_tr_b(0x0531 + testCaseOffset, "Fread - v. bl.: 5, 127", res);

    DWORD arr2[3] = {127, 256, 512};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr2, 3, rdBfr);
    out_tr_b(0x0532 + testCaseOffset, "Fread - v. bl.: 127, 256, 512", res);

    DWORD arr3[3] = {250, 512, 513};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr3, 3, rdBfr);
    out_tr_b(0x0533 + testCaseOffset, "Fread - v. bl.: 250, 512, 513", res);

    DWORD arr4[4] = {250, 550, 10, 1023};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr4, 4, rdBfr);
    out_tr_b(0x0534 + testCaseOffset, "Fread - v. bl.: 250, 550, 10, 1023", res);

    DWORD arr5[4] = {1023, 2048, 5000, 100};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr5, 4, rdBfr);
    out_tr_b(0x0535 + testCaseOffset, "Fread - v. bl.: 1023, 2048, 5000, 100", res);

    DWORD arr6[4] = {15000, 2000, 10, 2000};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr6, 4, rdBfr);
    out_tr_b(0x0536 + testCaseOffset, "Fread - v. bl.: 15000, 2000, 10, 2000", res);
}

void test054x(void)
{
    if((tosVersion >> 8) < 3) {     // if TOS version is less than 3 (it's and ST, not TT or Falcon)
        out_s("Tests 054x, 055x, 056x will be skipped as they are for TT RAM.");
        return;
    } else {
        out_s("Tests 054x, 055x, 056x - Fread() into TT RAM");
    }
    
    BYTE *bufTT = (BYTE *) Mxalloc(BUFFERSIZE, 1);
    
    if(bufTT == 0) {
        out_s("Tests 054x, 055x, 056x will be skipped - failed to allocate TT RAM.");
        return;
    }
    
    test051x(0x0030, bufTT);
    Mfree(bufTT);
}

void test050x(void)
{
    BYTE ok;
    WORD res1, res2, res3;

    Fdelete("TESTFILE");
    res1 = filenameExists("TESTFILE");
    res2 = Fcreate("TESTFILE", 0);
    res3 = filenameExists("TESTFILE");
    (res1 == 0 && res2 > 0 && res3 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0501, "Fcreate - non-existing file", ok, res2);
    
    res1 = Fclose(res2);
    (res1 == 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0502, "Fclose  - valid handle", ok, res1);
    
    res1 = Fopen("TESTFILE", 0);
    (res1 > 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0503, "Fopen   - existing file", ok, res1);
    if(res1 > 0) {
        Fclose(res1);
    }
    
    res1 = filenameExists("TESTFILE");
    res2 = Fcreate("TESTFILE", 0);
    res3 = filenameExists("TESTFILE");
    (res1 == 1 && res2 > 0 && res3 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0504, "Fcreate - existing file", ok, res2);
    (void) Fclose(res2);
    
    res1 = Fclose(res2);
    (res1 == 0xffdb) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0505, "Fclose  - invalid handle", ok, res1);        // steem drive returns 0 even though it should return some error code
    Fdelete("TESTFILE");
    
    res1 = Fopen("TESTFILE", 0);
    (res1 == 0xffdf) ? (ok = 1) : (ok = 0);                         // steem drive creates the file instead of returning error
    out_tr_bw(0x0506, "Fopen   - non-existing file", ok, res1);
    
    if(res1 > 0) {                                                  // if the file was opened (even though it shouldn't be), close it and delete it
        Fclose(res1);
        Fdelete("TESTFILE");
    }
    
    int h[256], i;
    char filename[16];
    int maxHandles = 256;
    for(i=0; i<256; i++) {
        strcpy(filename, "FILE");
        byteToHex((BYTE) i, filename + 4);
        h[i] = Fcreate(filename, 0);
        if(h[i] < 0) {
            maxHandles = i;
            break;
        }
    }
    
    (maxHandles > 20) ? (ok = 1) : (ok = 0);                        // steem drive allows 40 handles, native drive under TOS 2.06 allows 75 handles
    out_tr_bw(0x0507, "Fcreate - maximum handles", ok, maxHandles);

    for(i=0; i<=maxHandles; i++) {
        strcpy(filename, "FILE");
        byteToHex((BYTE) i, filename + 4);
        Fclose(h[i]);
        Fdelete(filename);
    }
}

BYTE testFreadByBlockSizeArray(DWORD size, WORD xorVal, DWORD *blockSizeArray, WORD blockSizeCount, BYTE *rdBfr)
{
    int i, f, res;
    f = openFile(0xABCD);
    
    if(f < 0) {
        return 0;
    }
    
    char canaries[32] = {"Herpy derpy this is a canary!!!"};

    int bIndex = 0;
    int good = 1;
    
    for(i=0; i<size; ) {
        int blockSize = blockSizeArray[bIndex];                     // get next block size
        bIndex++;                                                   // move to next block size
        if(bIndex >= blockSizeCount) {                              // if we're out of block sizes, then wrap around to 0th 
            bIndex = 0;
        }
    
        int readBlockSize = ((i + blockSize) <= size) ? blockSize : (size - i);     // block would go out of valid data? if not, use the block size, otherwise use the remaining size
        
        memset(rdBfr + i, 0, readBlockSize);                        // clear the memory
        memcpy(rdBfr + i + readBlockSize, canaries, 32);            // copy canaries beyond the expected end

        res = Fread(f, readBlockSize, rdBfr + i);                   // try to read
        
        if(res != readBlockSize) {                                  // didn't read everything? fail
            good = 0;
            break;
        }
        
        res = memcmp(buf1 + i, rdBfr + i, readBlockSize);           // compare buffers
        
        if(res != 0) {                                              // not matching? fail
            good = 0;
            break;
        }
        
        res = memcmp(rdBfr + i + readBlockSize, canaries, 32);      // check if canaries are alive

        if(res != 0) {                                              // not matching? fail
            good = 0;
            break;
        }        
        
        i += blockSize;                                             // move forward in tested file
    }
 
    Fclose(f);
    return good;
}

BYTE testFwriteByBlockSizeArray(DWORD size, DWORD *blockSizeArray, WORD blockSizeCount, BYTE *wrBfr)
{
    int i, f, res;
    Fdelete("WRITETST");
    f = Fcreate("WRITETST", 0);
    
    if(f < 0) {
        return 0;
    }
    
    int bIndex = 0;
    int good = 1;
    
    for(i=0; i<size; ) {
        int blockSize = blockSizeArray[bIndex];                     // get next block size
        bIndex++;                                                   // move to next block size
        if(bIndex >= blockSizeCount) {                              // if we're out of block sizes, then wrap around to 0th 
            bIndex = 0;
        }
    
        int writeBlockSize = ((i + blockSize) <= size) ? blockSize : (size - i);     // block would go out of valid data? if not, use the block size, otherwise use the remaining size
        
        res = Fwrite(f, writeBlockSize, wrBfr + i);                   // try to write
        
        if(res != writeBlockSize) {                                  // didn't read everything? fail
            good = 0;
            break;
        }
        
        i += blockSize;                                             // move forward in tested file
    }
    Fclose(f);
    f = -1;
    
    //---------------
    // verify the file by reading it back
    if(good) {
        f = Fopen("WRITETST", 0);
        
        if(f < 0) {                                                 // failed to open the file?
            good = 0;
        } else {
            res = Fread(f, size, buf2);                                 
            if(res != size) {                                       // if didn't read enough, fail
                good = 0;
            }
            
            res = memcmp(wrBfr, buf2, size);                        // compare the original and read buffer
            if(res != 0) {                                          // data mismatch? fail
                good = 0;
            }
            
            Fclose(f);
        }
    }
    
    Fdelete("WRITETST");
    return good;
}

BYTE testFreadByBlockSize(DWORD size, WORD xorVal, DWORD blockSize, BYTE *rdBfr)
{
    return testFreadByBlockSizeArray(size, xorVal, &blockSize, 1, rdBfr);
}

BYTE verifySeek(DWORD offset, int handle)
{
    int res = Fread(handle, 16, buf2 + offset);         // try to read
        
    if(res != 16) {                                     // didn't read everything? fail
        return 0;
    }
        
    Fseek(-16, handle, SEEK_CUR);                       // move back, so we don't screw up the current pointer in file        
        
    res = memcmp(buf1 + offset, buf2 + offset, 16);     // compare buffers
        
    if(res != 0) {                                      // not matching? fail
        return 0;
    }
    
    return 1;
}

void fillBuffer(DWORD size, WORD xorVal, BYTE *bfr)
{
    DWORD i;
    WORD val;
    WORD counter = 0;
    for(i=0; i < size; i += 2) {
        val = counter ^ xorVal;
        val = val ^ (val << 8);
        counter++;
        
        bfr[i + 0] = (BYTE) (val >> 8);
        bfr[i + 1] = (BYTE) (val     );
    }
    
    if(size & 1) {                      // odd length? make the zero value after last valid value 
        bfr[size] = 0;
    }
}

int openFile(WORD xorVal)
{
    char fname[16];
    strcpy   (fname,  "FILE");
    wordToHex(xorVal, fname + 4);
    
    int res = Fopen(fname, 0);
    return res;
}

void deleteFile(WORD xorVal)
{
    char fname[16];
    strcpy   (fname,  "FILE");
    wordToHex(xorVal, fname + 4);
    
    Fdelete(fname);
}

BYTE createFile(DWORD size, WORD xorVal)
{
    fillBuffer(size, xorVal, buf1);     // create buffer

    char fname[16];
    strcpy   (fname,  "FILE");
    wordToHex(xorVal, fname + 4);
    
    int f = Fcreate(fname, 0);
    if(f < 0) {                         // couldn't create file? fail
        return 0;
    }
    
    int res = Fwrite(f, size, buf1);    // write all the data
    Fclose(f);
    
    if(res != size) {                   // didn't write all the bytes? fail
        return 0;
    }
    
    return 1;                           // good
}
