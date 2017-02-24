#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

#define  BYTE unsigned char
#define  WORD unsigned short
#define DWORD unsigned int

void createEmptyImage(void)
{
    printf("Creating image.\n");

    FILE *f = fopen("hdd.img", "wb");
    if(!f) {
        printf("Failed to create file\n");
        return;
    }

    char data[512];
    memset(data, 0, 512);

    #define SIZE1MB     (1024 * 1024)
    #define SIZE10MB    (10   * SIZE1MB)

    int sectorCount = (260 * SIZE1MB) / 512;

    int sectorNo;
    for(sectorNo=0; sectorNo<sectorCount; sectorNo++) { // write all the sectors
        fwrite(data, 1, 512, f);

        int wroteBytes = sectorNo * 512;

        if(wroteBytes % SIZE10MB == 0) {
            printf("Wrote %d MB\n", wroteBytes / SIZE1MB);
        }        
    }

    fclose(f);
    printf("Done.\n");
}

void mapUsedSectors(void)
{
    printf("Mapping used sectors image.\n");

    FILE *f = fopen("hdd.img", "rb");
    if(!f) {
        printf("Failed to open file\n");
        return;
    }

    BYTE data[512];
    
    int sectorsUsed = 0;
    int sectorsFree = 0;

    int sector = 0;
    while(!feof(f)) {
        size_t readCnt = fread(data, 1, 512, f);

        if(readCnt < 512) {
            continue;
        }

        int isUsed = 0;
        int i;
        for(i=0; i<512; i++) {
            if(data[i] != 0) {
                isUsed = 1;
                break;
            }
        }

        if(isUsed) {
            printf("Used sector: % 4d (%04x): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", sector, sector, 
                    data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9]);

            sectorsUsed++;
        } else {
            sectorsFree++;
        }

        sector++;
    }

    fclose(f);
    printf("Sectors used: %d\n", sectorsUsed);
    printf("Sectors free: %d\n", sectorsFree);
    printf("Done.\n\n");
}

typedef struct __attribute__((__packed__)) {
    BYTE status;
    BYTE firstHead;
    BYTE firstCylSector;
    BYTE firstCylinder;

    BYTE partType;
    BYTE lastHead;
    BYTE lastCylSector;
    BYTE lastCylinder;

    DWORD firstLBA;
    DWORD sectorCount;
} PTE;

typedef struct __attribute__((__packed__)) {
    BYTE  flags;
    BYTE  partType[3];
    DWORD partStart;
    DWORD partSize;
} APH;

typedef struct __attribute__((__packed__)) {
    BYTE jumpInst[3];
    BYTE oemId[8];

    struct __attribute__((__packed__)) {
        WORD  bps;
        BYTE  spc;
        WORD  reservedSects;
        BYTE  numOfFATs;
        WORD  rootEntries;
        WORD  smallSectors;
        BYTE  mediaDescr;
        WORD  sectorsPerFAT;
        WORD  spt;
        WORD  numOfHeads;
        DWORD hiddenSectors;
        DWORD largeSectors;
    } BPB;

    struct __attribute__((__packed__)) {
        BYTE  physDriveNo;
        BYTE  reserved;
        BYTE  extBootSign;
        DWORD serialNo;
        BYTE  volumeLabel[11];
        BYTE  fileSystemType[8];
    } extBPBP;
    
} FAT16BS;

typedef struct __attribute__((__packed__)) {
    BYTE ignore[11];
    WORD bps;
    BYTE spc;
    WORD res;
    BYTE nfats;
    WORD ndirs;
    WORD nsects;
    BYTE media;
    WORD spf;
    WORD spt;
    WORD nsides;
    WORD nhid;
} AtariBootsector;

void reverseDword(DWORD *dw)
{
    BYTE *pB = (BYTE *) dw;

    BYTE a,b,c,d;
    a = pB[0];
    b = pB[1];
    c = pB[2];
    d = pB[3];

    pB[0] = d;
    pB[1] = c;
    pB[2] = b;
    pB[3] = a;
}

void analyzeBootSector(void)
{
    printf("\n\nAnalyzing boot sector...\n");

    FILE *f = fopen("hdd.img", "rb");
    if(!f) {
        printf("Failed to open file\n");
        return;
    }

    char data[512];

    fseek(f, 0L, SEEK_SET);             // to sector #0
    fread(data, 1, 512, f);

    PTE *pPte = (PTE *) &data[0x1BE];

    //----------------
    // show PC partition Entry

    WORD cyl, sec;
    printf("PC Partition Entry #1:\n");
    printf("status       : %02x\n", pPte->status);
    
    cyl = ((pPte->firstCylSector & 0xc0)<<2) | pPte->firstCylinder;
    sec =  (pPte->firstCylSector & 0x3f);
    printf("1st  C       : %03x\n", cyl);
    printf("1st  H       : %02x\n", pPte->firstHead);
    printf("1st  S       : %02x\n", sec);

    printf("part type    : %02x\n", pPte->partType);

    cyl = ((pPte->lastCylSector & 0xc0)<<2) | pPte->lastCylinder;
    sec =  (pPte->lastCylSector & 0x3f);
    printf("last C       : %03x\n", cyl);
    printf("last H       : %02x\n", pPte->lastHead);
    printf("last S       : %02x\n", sec);
    printf("LBA 1st sect : %08x\n", pPte->firstLBA);
    printf("LBA sect cnt : %08x sectors (%d MB)\n", pPte->sectorCount, (pPte->sectorCount * 512) / (1024 * 1024));

    printf("\n\n");

    //----------------
    // show Atari partition header
    APH *pAph = (APH *) &data[0x1DE];

    reverseDword(&pAph->partStart);
    reverseDword(&pAph->partSize);

    printf("Atari Partition Header #2:\n");
    printf("flags        : %02x\n", pAph->flags);
    printf("part type    : %.3s\n", pAph->partType);
    printf("part start   : %08x\n", pAph->partStart);
    printf("part size    : %08x sectors (%d MB)\n", pAph->partSize, (pAph->partSize * 512) / (1024 * 1024));

    printf("\n\n");

    //----------------
    // show 0th PC partition sector
    int offset;
    int firstPcSector       = pPte->firstLBA;
    int firstAtariSector    = pAph->partStart;

    offset = firstPcSector * 512;
    fseek(f, offset, SEEK_SET);     // to 0th PC partition sector
    fread(data, 1, 512, f);

    FAT16BS *pF16 = (FAT16BS *) data;

    printf("PC partition sector 0 (physical sector %08x):\n", firstPcSector);

    printf("jump instruct: %02x%02x%02x\n", pF16->jumpInst[0], pF16->jumpInst[1], pF16->jumpInst[2]);
    printf("oemId        : %.8s\n",  pF16->oemId);
    printf("bytes/sector : %d\n",    pF16->BPB.bps);
    printf("sectors/clust: %d\n",    pF16->BPB.spc);
    printf("reservedSects: %d\n",    pF16->BPB.reservedSects);
    printf("num Of FATs  : %d\n",    pF16->BPB.numOfFATs);
    printf("root entries : %d\n",    pF16->BPB.rootEntries);
    printf("small sectors: %04x\n",  pF16->BPB.smallSectors);
    printf("media descr  : %02x\n",  pF16->BPB.mediaDescr);
    printf("sectors/FAT  : %04x\n",  pF16->BPB.sectorsPerFAT);
    printf("sectors/track: %04x\n",  pF16->BPB.spt);
    printf("# of heads   : %04x\n",  pF16->BPB.numOfHeads);
    printf("hidden sects : %08x\n",  pF16->BPB.hiddenSectors);
    printf("large  sects : %08x\n",  pF16->BPB.largeSectors);
    printf("phys driveNo : %02x\n",  pF16->extBPBP.physDriveNo);
    printf("reserved     : %02x\n",  pF16->extBPBP.reserved);
    printf("extBootSign  : %02x\n",  pF16->extBPBP.extBootSign);
    printf("serial #     : %08x\n",  pF16->extBPBP.serialNo);
    printf("volume label : %.11s\n", pF16->extBPBP.volumeLabel);
    printf("filesys type : %.8s\n",  pF16->extBPBP.fileSystemType);

    printf("\n\n");

    //----------------
    // show 0th Atari partition sector
    offset = firstAtariSector * 512;
    fseek(f, offset, SEEK_SET);     // to 0th Atari partition sector
    fread(data, 1, 512, f);

    AtariBootsector *pAbs = (AtariBootsector *) data;

    printf("Atari partition sector 0 (physical sector %08x):\n", firstAtariSector);

    printf("bytes/sector : %d\n", pAbs->bps);
    printf("sects/cluster: %d\n", pAbs->spc);
    printf("reserved sect: %d\n", pAbs->res);
    printf("n of FATs    : %d\n", pAbs->nfats);
    printf("root entries : %d\n", pAbs->ndirs);
    printf("n of sectors : %d (%08x)\n", pAbs->nsects, pAbs->nsects);
    printf("media descr  : %02x\n", pAbs->media);
    printf("sectors/FAT  : %d\n", pAbs->spf);
    printf("sectors/track: %d\n", pAbs->spt);
    printf("sides        : %d\n", pAbs->nsides);
    printf("hidden sects : %d\n", pAbs->nhid);

    printf("\n\n");
    //----------------

    fclose(f);
}

void showMenu(void)
{
    printf("Menu:\n");
    printf("Q - quit\n");
    printf("C - create empty image\n");
    printf("M - map which sectors are used\n");
    printf("B - analyze boot sector\n");
    printf("Please press a key (and enter)\n");    
}

int main(void)
{
    printf("\n\nSD partition tool.\n");

    showMenu();

    while(1) {
        int validKey = 0;
        int key = tolower(getchar());

        if(key == 'q') {
            break;
        }

        if(key == 'c') {
            createEmptyImage();
            validKey = 1;
        }

        if(key == 'm') {
            mapUsedSectors();
            validKey = 1;
        }

        if(key == 'b') {
            analyzeBootSector();
            validKey = 1;
        }

        if(validKey) {
            showMenu();
        }
    }

    return 0;
}

