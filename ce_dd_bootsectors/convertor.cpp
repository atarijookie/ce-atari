#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTE    unsigned char
#define WORD    unsigned short
#define DWORD   unsigned int

WORD swapNibbles(WORD val);

bool createBootsectorFromPrg(char *path, char *inFile, char *outFile, bool bootsectorNotRaw);
void checkCeddSize(void);
DWORD getValue(BYTE *p);
void createImage(void);

int main(int argc, char *argv[])
{
    char *path = "c:\\!nohaj\\tmp\\assembla_atarijookie\\trunk\\ce_dd_bootsectors";
    createBootsectorFromPrg(path, (char *) "bs_st.prg", (char *) "ce_dd_st.bs", true);
    createBootsectorFromPrg(path, (char *) "bs_tt.prg", (char *) "ce_dd_tt.bs", true);
    createBootsectorFromPrg(path, (char *) "bs_fn.prg", (char *) "ce_dd_fn.bs", true);
    createBootsectorFromPrg(path, (char *) "bs_l2.prg", (char *) "ce_dd_l2.bs", false);

/*
    checkCeddSize();
    createImage();
*/

    getchar();
    return 0;
}

bool createBootsectorFromPrg(char *path, char *inFile, char *outFile, bool bootsectorNotRaw)
{
    printf("\nConvert: %s -> %s\n", inFile, outFile);

    char bfr[512];
    memset(bfr, 0, 512);

    bfr[0] = 0x60;              // store magic word
    bfr[1] = 0x1c;

    char fullInFile [256];
    char fullOutFile[256];
    memset(fullInFile, 0, 256);
    memset(fullOutFile, 0, 256);

    strcpy(fullInFile, path);
    strcat(fullInFile, "\\");
    strcat(fullInFile, inFile);

    strcpy(fullOutFile, path);
    strcat(fullOutFile, "\\");
    strcat(fullOutFile, outFile);

    //---------------------------------------
    // read the boot code to buffer
    FILE *bc = fopen(fullInFile, "rb");
    if(!bc) {
        printf("\nCould not open INPUT file: %s\n", fullInFile);
        return false;
    }

    fseek (bc, 0, SEEK_END);        // move to end
    int fsize = ftell(bc) - 256;    // get file size

    fseek(bc, 256, SEEK_SET);       // skip TOS prog header

    int offset  = 0x1e;
    int len     = 512 - 32;
    if(!bootsectorNotRaw) {         // if it's raw sector, the offset and length are different
        offset  = 0;
        len     = 512;
    }

    int cnt = fread(&bfr[offset], 1, len, bc);

    if(fsize > len) {
        printf("\nFile is bigger than what we can use! Is the bootcode too long? (usable space: %d, this file size: %d)\n", len, fsize);
    }

    fclose(bc);
    //---------------------------------------
    // create the check sum
    WORD sum = 0, val;
    WORD *p = (WORD *) bfr;

    for(int i=0; i<255; i++) {
        val = *p;
        val = swapNibbles(val);
        sum += val;
        p++;
    }

    if(bootsectorNotRaw) {          // do checksum only if not raw
        WORD cs = 0x1234 - sum;
        sum = sum & 0xffff;

        printf("sum %04x, check-sum is %04x, check is %04x, free space: %d bytes\n", sum, cs, (cs + sum) & 0xffff, len - cnt);

        bfr[510] = cs >> 8;         // store the check sum
        bfr[511] = cs;
    }

    //---------------------------------------
    // now write it to file
    FILE *bs = fopen(fullOutFile, "wb");

    if(!bs) {
        printf("\nCould not open OUTPUT file: %s\n", fullOutFile);
        return false;
    }

    fwrite(bfr, 1, 512, bs);
    fclose(bs);

    printf("Done   : %s -> %s\n", inFile, outFile);
    return true;
}

void createImage(void)
{
    BYTE bfr[1024*1024];

    // open output file
    FILE *out = fopen("ceddboot.img", "wb");

    if(!out) {
        printf("\ncreateImage - could not open OUTPUT file!\n");
        getchar();
        return;
    }

    DWORD cnt = 0;

    //--------------------------
    // read and write bootsector
    FILE *in = fopen("bootsect.bin", "rb");
    if(!in) {
        fclose(out);
        printf("\ncreateImage - could not open bootsector file (001)!\n");
        getchar();
        return;
    }


    int res = fread(bfr, 1, 512, in);             // read and write bootsector
    if(res >= 0) {
        cnt += res;
    }
    fclose(in);

    fwrite(bfr, 1, 512, out);

    //--------------------------
    // read and write the driver
    in = fopen("ce_dd.prg", "rb");
    if(!in) {
        fclose(out);
        printf("\ncreateImage - could not open driver file (002)!\n");
        getchar();
        return;
    }

    res = fread(bfr, 1, 1024*1024, in);             // read and write driver
    if(res >= 0) {
        cnt += res;
    }
    fclose(in);

    fwrite(bfr, 1, res, out);

    //--------------------------
    // pad with zeros to make full sectors
    int mod = cnt % 512;
    int add = 1024 - mod;

    memset(bfr, 0, add);
    fwrite(bfr, 1, add, out);

    //--------------------------
    // pad to 5MB for STEEM Pasti
    memset(bfr, 0, 1024*1024);

    for(int i=0; i<5; i++) {
        fwrite(bfr, 1, 1024*1024, out);
    }

    fclose(out);

    printf("\nCEDDBOOT.IMG created.\n");
}

void checkCeddSize(void)
{
    FILE *f = fopen("ce_dd.prg", "rb");

    if(!f) {
        printf("\nCould not open ce_dd.prg file!\n");
        getchar();
        return;
    }

    char bfr[20];
    fread(bfr, 1, 20, f);

    fclose(f);

    DWORD tsize, dsize, bsize;

    tsize = getValue((BYTE *)(bfr + 2));
    dsize = getValue((BYTE *)(bfr + 6));
    bsize = getValue((BYTE *)(bfr + 10));

    printf("\n\nCEDD driver \n");
    printf("text  size: %d\n", tsize);
    printf("data  size: %d\n", dsize);
    printf("bss   size: %d\n", bsize);
    printf("TOTAL size: %d\n", tsize + dsize + bsize);
}

DWORD getValue(BYTE *p)
{
    DWORD val = 0;

    val = ((DWORD) p[0]);
    val = val << 8;
    val = val  | ((DWORD) p[1]);
    val = val << 8;
    val = val  | ((DWORD) p[2]);
    val = val << 8;
    val = val  | ((DWORD) p[3]);

    return val;
}

WORD swapNibbles(WORD val)
{
    WORD a,b;

    a = val >> 8;           // get upper
    b = val &  0xff;        // get lower

    return ((b << 8) | a);
}
