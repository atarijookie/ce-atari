#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

void createEmptyImage(void)
{
    printf("Creating image.\n");

    FILE *f = fopen("hdd.img", "wb");
    if(!f) {
        printf("Failed to create file\n");
        return;
    }

    char sector[512];
    memset(sector, 0, 512);

    int i;
    for(i=0; i<((260*1024*1024)/512); i++) {        // write all the sectors
        fwrite(sector, 1, 512, f);

        if(i % ((1024*1024)/512) == 0) {
            printf("Wrote %d MB\n", (i * 512) / (1024*1024));
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

    char data[512];
    
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
            printf("Used sector: % 4d (%04x)\n", sector, sector);
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

#define  BYTE unsigned char
#define  WORD unsigned short
#define DWORD unsigned int


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

    printf("Partition entry #1:\n");
    printf("status       : %02x\n", pPte->status);
    printf("1st  C       : %02x\n", pPte->firstCylSector);
    printf("1st  H       : %02x\n", pPte->firstHead);
    printf("1st  S       : %02x\n", pPte->firstCylinder);
    printf("part type    : %02x\n", pPte->partType);
    printf("last C       : %02x\n", pPte->lastCylSector);
    printf("last H       : %02x\n", pPte->lastHead);
    printf("last S       : %02x\n", pPte->lastCylinder);
    printf("LBA 1st sect : %08x\n", pPte->firstLBA);
    printf("LBA sect cnt : %08x\n", pPte->sectorCount);

    printf("\n\n", pPte->sectorCount);
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

