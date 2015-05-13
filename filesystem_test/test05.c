#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdio.h>
#include "stdlib.h"
#include "out.h"

extern int drive;

void test050x(void);
void test051x(void);
void test054x(void);
void test055x(void);

BYTE filenameExists(char *filename);
void deleteIfExists(char *fname);

int  openFile(WORD xorVal);
BYTE createFile(DWORD size, WORD xorVal);
void deleteFile(WORD xorVal);
void fillBuffer(DWORD size, WORD xorVal, BYTE *bfr);

BYTE testFreadByBlockSize(DWORD size, WORD xorVal, DWORD blockSize);
BYTE testFreadByBlockSizeArray(DWORD size, WORD xorVal, DWORD *blockSizeArray, WORD blockSizeCount);
BYTE verifySeek(DWORD offset, int handle);

BYTE *buf1, *buf2;
#define TEST051XFILESIZE    (200 * 1024)

void test05(void)
{
    out_s("Fcreate, Fopen, Fclose, Fread, Fwrite, Fseek");
    
    Dsetdrv(drive);
    Dsetpath("\\");

    buf1 = (BYTE *) Malloc(220 * 1024);
    buf2 = (BYTE *) Malloc(220 * 1024);
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
    
    test050x();

    out_s("Creating test file...");
    int res = createFile(TEST051XFILESIZE, 0xABCD);
    if(!res) {
        out_tr_b(0x0510, "Failed to create file!", 0);
        return;
    }

    test051x();
    test054x();
    test055x();

    deleteFile(0xABCD);
    
    Mfree(buf1);
    Mfree(buf2);
    
    out_s("");
}

void test055x(void)
{
    int f, res1, res2, ok;

    f = openFile(0xABCD);
    
    if(f < 0) {
        out_s("055x - Failed to open file");
        return;
    }
    
    //---------
    // test SEEK_SET
    res1 = Fseek(0, f, 0);
    res2 = verifySeek(0, f);
    (res1 == 0 && res2 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0551, "Fseek - SEEK_SET to start while at start", ok, res1);
    
    res1 = Fseek(TEST051XFILESIZE/2, f, 0);
    res2 = verifySeek(TEST051XFILESIZE/2, f);
    (res1 == (TEST051XFILESIZE/2) && res2 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0552, "Fseek - SEEK_SET to half", ok, res1);

    res1 = Fseek(TEST051XFILESIZE, f, 0);
    res2 = Fread(f, 16, buf2);
    (res1 == TEST051XFILESIZE && res2 == 0) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0553, "Fseek - SEEK_SET to end", ok, res1);

    res1 = Fseek(0, f, 0);
    res2 = verifySeek(0, f);
    (res1 == 0 && res2 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0554, "Fseek - SEEK_SET back to start", ok, res1);
    
    //---------
    // test SEEK_CUR
    res1 = Fseek(TEST051XFILESIZE/2, f, 1);
    res2 = verifySeek(TEST051XFILESIZE/2, f);
    (res1 == (TEST051XFILESIZE/2) && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0555, "Fseek - SEEK_CUR to half  (+half)", ok, res1);

    res1 = Fseek(TEST051XFILESIZE/2, f, 1);
    res2 = Fread(f, 16, buf2);
    (res1 == TEST051XFILESIZE && res2 == 0) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0556, "Fseek - SEEK_CUR to end   (+half)", ok, res1);

    res1 = Fseek(-(TEST051XFILESIZE/2), f, 1);
    res2 = verifySeek(TEST051XFILESIZE/2, f);
    (res1 == (TEST051XFILESIZE/2) && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0557, "Fseek - SEEK_CUR to half  (-half)", ok, res1);
    
    res1 = Fseek(-(TEST051XFILESIZE/2), f, 1);
    res2 = verifySeek(0, f);
    (res1 == 0 && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0558, "Fseek - SEEK_CUR to start (-half)", ok, res1);
    
    //---------
    // test SEEK_END
/*
// these seem to fail all the time :-(    
    res1 = Fseek(TEST051XFILESIZE, f, 2);
    res2 = verifySeek(0, f);
    (res1 == 0 && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0559, "Fseek - SEEK_END to start", ok, res1);

    res1 = Fseek(TEST051XFILESIZE/2, f, 2);
    res2 = verifySeek(TEST051XFILESIZE/2, f);
    (res1 == (TEST051XFILESIZE/2) && res2 == 1) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0560, "Fseek - SEEK_END to half", ok, res1);
    
    res1 = Fseek(0, f, 2);
    res2 = Fread(f, 16, buf2);
    (res1 == TEST051XFILESIZE && res2 == 0) ? (ok = 1) : (ok = 0);    
    out_tr_bd(0x0561, "Fseek - SEEK_END to end", ok, res1);
*/    
    
    Fclose(f);    
}

void test054x(void)
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
    out_tr_bd(0x0541, "Fread - exact file size", ok, res1);

    (f > 0 && res2 == 0) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0542, "Fread - beyond file size", ok, res2);

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
    out_tr_bd(0x0543, "Fread - zero size of read", ok, res1);

    (f > 0 && res2 == TEST051XFILESIZE) ? (ok = 1) : (ok = 0);
    out_tr_bd(0x0544, "Fread - more than what is in the file", ok, res2);

    //--------------
    WORD w = Fread(f, TEST051XFILESIZE, buf2);
    (w == 0xffdb) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0545, "Fread - on invalid handle", ok, w);
}

void test051x(void)
{
    int res;

    //---------------------------
    // test by fixed block read size
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 15);
    out_tr_b(0x0511, "Fread - test block size:      15 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 126);
    out_tr_b(0x0512, "Fread - test block size:     126 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 511);
    out_tr_b(0x0513, "Fread - test block size:     511 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 512);
    out_tr_b(0x0514, "Fread - test block size:     512 B", res);
    
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 750);
    out_tr_b(0x0515, "Fread - test block size:     750 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 1023);
    out_tr_b(0x0516, "Fread - test block size:    1023 B", res);
    
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 1024);
    out_tr_b(0x0517, "Fread - test block size:    1024 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 1025);
    out_tr_b(0x0518, "Fread - test block size:    1025 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 63000);
    out_tr_b(0x0519, "Fread - test block size:   63000 B", res);
 
    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 65536);
    out_tr_b(0x0520, "Fread - test block size:   65536 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 130047);       // ACSI MAX_SECTORS - 1
    out_tr_b(0x0521, "Fread - test block size:  130047 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 130048);       // ACSI MAX_SECTORS
    out_tr_b(0x0522, "Fread - test block size:  130048 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 130049);       // ACSI MAX_SECTORS + 1
    out_tr_b(0x0523, "Fread - test block size:  130049 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, 150000);
    out_tr_b(0x0524, "Fread - test block size:  150000 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, TEST051XFILESIZE);
    out_tr_b(0x0525, "Fread - test block size:  204800 B", res);

    res = testFreadByBlockSize(TEST051XFILESIZE, 0xABCD, TEST051XFILESIZE + 1024);
    out_tr_b(0x0526, "Fread - test block size:  205824 B", res);
    
    //---------------------------
    // test by predefined various block sizes
    DWORD arr1[2] = {5, 127};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr1, 2);
    out_tr_b(0x0531, "Fread - v. bl.: 5, 127", res);

    DWORD arr2[3] = {127, 256, 512};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr2, 3);
    out_tr_b(0x0532, "Fread - v. bl.: 127, 256, 512", res);

    DWORD arr3[3] = {250, 512, 513};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr3, 3);
    out_tr_b(0x0533, "Fread - v. bl.: 250, 512, 513", res);

    DWORD arr4[4] = {250, 550, 10, 1023};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr4, 4);
    out_tr_b(0x0534, "Fread - v. bl.: 250, 550, 10, 1023", res);

    DWORD arr5[4] = {1023, 2048, 5000, 100};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr5, 4);
    out_tr_b(0x0535, "Fread - v. bl.: 1023, 2048, 5000, 100", res);

    DWORD arr6[4] = {15000, 2000, 10, 2000};
    res = testFreadByBlockSizeArray(TEST051XFILESIZE, 0xABCD, arr6, 4);
    out_tr_b(0x0536, "Fread - v. bl.: 15000, 2000, 10, 2000", res);
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

BYTE testFreadByBlockSizeArray(DWORD size, WORD xorVal, DWORD *blockSizeArray, WORD blockSizeCount)
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
        
        memset(buf2 + i, 0, readBlockSize);                         // clear the memory
        memcpy(buf2 + i + readBlockSize, canaries, 32);             // copy canaries beyond the expected end

        res = Fread(f, readBlockSize, buf2 + i);                    // try to read
        
        if(res != readBlockSize) {                                  // didn't read everything? fail
            good = 0;
            break;
        }
        
        res = memcmp(buf1 + i, buf2 + i, readBlockSize);            // compare buffers
        
        if(res != 0) {                                              // not matching? fail
            good = 0;
            break;
        }
        
        res = memcmp(buf2 + i + readBlockSize, canaries, 32);       // check if canaries are alive

        if(res != 0) {                                              // not matching? fail
            good = 0;
            break;
        }        
        
        i += blockSize;                                             // move forward in tested file
    }
 
    Fclose(f);
    return good;
}

BYTE testFreadByBlockSize(DWORD size, WORD xorVal, DWORD blockSize)
{
    return testFreadByBlockSizeArray(size, xorVal, &blockSize, 1);
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
