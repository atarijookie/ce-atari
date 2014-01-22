#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTE    unsigned char
#define WORD    unsigned short
#define DWORD   unsigned int

WORD swapNibbles(WORD val);

void checkCeddSize(void);
DWORD getValue(BYTE *p);

int main(int argc, char *argv[])
{
    char bfr[512];

    memset(bfr, 0, 512);

    bfr[0] = 0x60;              // store magic word
    bfr[1] = 0x1c;

    //---------------------------------------
    // read the boot code to buffer
    FILE *bc = fopen("bootsect.prg", "rb");
    if(!bc) {
        printf("\nCould not open INPUT file!\n");
        getchar();
        return 0;
    }

    fseek(bc, 256, SEEK_SET);       // skip TOS prog header
    fread(&bfr[0x1e], 1, 512 - 32, bc);

    if(!feof(bc)) {
        printf("\nDidn't hit the EOF when reading input file! Is the bootcode too long?\n");
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

    WORD cs = 0x1234 - sum;
    sum = sum & 0xffff;


    printf("sum %04x, check-sum is %04x, check is %04x", sum, cs, (cs + sum) & 0xffff);

    bfr[510] = cs >> 8;         // store the check sum
    bfr[511] = cs;

    //---------------------------------------
    // now write it to file
    FILE *bs = fopen("ceddboot.001", "wb");

    if(!bs) {
        printf("\nCould not open OUTPUT file!\n");
        getchar();
        return 0;
    }

    fwrite(bfr, 1, 512, bs);

    fclose(bs);
    //---------------------------------------
    checkCeddSize();

    getchar();
    return 0;
}

void checkCeddSize(void)
{
    FILE *f = fopen("ceddboot.002", "rb");

    if(!f) {
        printf("\nCould not open ceddboot.002 file!\n");
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
