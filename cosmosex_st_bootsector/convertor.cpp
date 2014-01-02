#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD    unsigned short
WORD swapNibbles(WORD val);

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

    printf("sum %04x, check-sum is %04x, check is %04x", sum, cs, (cs + sum));

    bfr[510] = cs >> 8;         // store the check sum
    bfr[511] = cs;

    //---------------------------------------
    // now write it to file
    FILE *bs = fopen("ceddboot.bin", "wb");

    if(!bs) {
        printf("\nCould not open OUTPUT file!\n");
        getchar();
        return 0;
    }

    fwrite(bfr, 1, 512, bs);

    fclose(bs);

    getchar();
    return 0;
}

WORD swapNibbles(WORD val)
{
    WORD a,b;

    a = val >> 8;           // get upper
    b = val &  0xff;        // get lower

    return ((b << 8) | a);
}
