#include <stdio.h>
#include <osbind.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BYTE  	unsigned char
#define WORD  	uint16_t
#define DWORD 	uint32_t

void showMenu(void);

void jumpReadTest(void);
void makeFloppy(void);

void readSector(int sector, int track, int side);
void writeSector(int sector, int track, int side, int doPrint);

void showBiosError(int errorNo);
void showDecimal(int num);
void showDiff(BYTE* bfr, int track, int side, int sector, char*range);

BYTE writeBfr[512];

int main(void)
{
	int track, sector, side;
	int no, i;
	char req;
    int readNotWrite;

    for(i=0; i<512; i++) {
        writeBfr[i] = (BYTE) i;
    }
	
	track	= 0;
	side	= 0;
	sector	= 1;
	
    showMenu();
    
	while(1) {
		req = Cnecin();
        readNotWrite = 1;                           // 1 means READ, 0 means WRITE
		
		/* should quit? */
		if(req == 'q' || req == 'Q' || req=='e' || req=='E') {	
			return 0;
		}
        
        if(req == 'M' || req == 'm') {
            showMenu();
            continue;
        }
		
        if(req == 'c' || req == 'C') {              // continuous jump + read test
            jumpReadTest();
            continue;
        }

        if(req == 'p' || req == 'P') {              // make floppy for jump + read test
            makeFloppy();
            continue;
        }
        
		if(req == 't' || req == 'T') {				/* set track # */
			no = ((BYTE) Cnecin()) - 48;
			if(no >= 0 && no < 85) {
				track = no;
			}
		} else if(req == 'i' || req == 'I') {		/* set side # */
			no = ((BYTE) Cnecin()) - 48;
			if(no >= 0 && no <= 1) {
				side = no;
			}
		} else if(req == 's' || req == 'S') {		/* set sector # */
			no = ((BYTE) Cnecin()) - 48;
			if(no >= 1 && no <= 12) {
				sector = no;
			}
		} else if(req == 'j' || req == 'J') {		/* jump up by 20 track */
			if((track + 20) < 80) { 
				track += 20;
			} else {
				track = 0;
			}			
			printf("Jumped up to track %d\n", track);
		} else if(req == 'k' || req == 'K') {		/* jump down by 20 track */
			if((track - 20) >= 0) { 
				track -= 20;
			} else {
				track = 79;
			}			
			printf("Jumped down to track %d\n", track);
		} else if(req == 'r' || req == 'R') {		/* jump down by 20 track */
			track	= 0;
			side	= 0;
			sector	= 1;
			printf("Reset to track 0, side 0, sector 1\n");
		} else if(req == 'w' || req == 'W') {       // write sector
            readNotWrite = 0;
        }        
        
        if(readNotWrite == 1) {                     // 1 means READ, 0 means WRITE
            readSector(sector, track, side);
        } else {
            writeSector(sector, track, side, 1);
        }
	}
}

void showMenu(void)
{
	printf("%c%cManual floppy sector requests tool.\n", 27, 69);
    printf("Commands: 'q' to quit, set sector: 's1',\n");
    printf("set side: 'i0', set track: 't5'\n");
    printf("'j' - jump up, 'k' - jump down, 'r' - go to track 0\n");
    printf("'c' - continuous jump + read test, 'p' - write the jump + read floppy\n");
    printf("'w' - write currently set sector, 'm' - this menu\n\n");
}

void makeFloppy(void)
{
    int sector, track, side;
    
    printf("Writing whole floppy...\n");
    
    for(track=0; track<80; track++) {
        printf("\nTrack %02d: ", track);

        for(side=0; side<2; side++) {
            for(sector=1; sector<=10; sector++) {
                writeSector(sector, track, side, 0);
                printf(".");
            }
        }
    }
}

void jumpReadTest(void)
{
  	BYTE bfr[512];
	int res;
    int sector, track, side;
    char range[]="#123456789ABCDEFG";
    BYTE initval=0;

    printf("\nContinuous read + jump test\n");

    while(1) {
        sector  = (rand() % 10)+1;
        side    = rand() % 2;
        track   = rand() % 80;
     
     	//just to make sure we are not looking at data from the last read attempt on a failure
        memset(bfr,initval++,512);

        res = Floprd(bfr, 0, 0, sector, track, side, 1);
	    printf("READ Track %02d, Side %d, Sector %d -- ", track, side, sector);

        if(res != 0) {
            printf("FAIL -- res = %d\n", res);
            showBiosError(res);
        } else {
            if(bfr[0] == track && bfr[1] == side && bfr[2] == sector) {
                printf("GOOD, TrSiSe GOOD\n");
            } else {
                printf("GOOD, TrSiSe BAD Tr%dSi%dSe%d\n",bfr[0],bfr[1],bfr[2]);
                showDiff(bfr, track, side, sector, range);
                Cnecin();
            }
        }
    }
}

void showDiff(BYTE* bfr, int track, int side, int sector, char*range)
{
	int diffcnt=0;
	int blockcnt=0;
	int i=0;

	if(bfr[0] != track) {
		diffcnt++;
	}
	if(bfr[1] != side) {
		diffcnt++;
	}
	if(bfr[2] != sector) {
		diffcnt++;
	}
	blockcnt=3;
	for( i=3; i<512; i++ ){
		if( bfr[i] != (BYTE)i) {
			diffcnt++;
		}
		blockcnt++;
		if( blockcnt==16 ){
			blockcnt=0;
			printf("%c",range[diffcnt]);
			diffcnt=0;
		}
	}
	printf("\n");
}

void readSector(int sector, int track, int side)
{
	BYTE bfr[512];
	int res;

	res = Floprd(bfr, 0, 0, sector, track, side, 1);
				
    printf("READ  Tr %d, Si %d, Se %d -- ", track, side, sector);

	if(res != 0) {
		printf("FAIL -- res = %d\n", res);
		showBiosError(res);
	} else {
		if(bfr[0] == track && bfr[1] == side && bfr[2] == sector) {
			printf("GOOD, TrSiSe GOOD\n");
		} else {
			printf("GOOD, TrSiSe BAD\n");
		}
	}
}

void writeSector(int sector, int track, int side, int doPrint)
{
	int res;

    // customize write data
    writeBfr[0] = track;
    writeBfr[1] = side;
    writeBfr[2] = sector;
    
    // issue write command
	res = Flopwr(writeBfr, 0, 0, sector, track, side, 1);
                
    if(doPrint) {
        printf("WRITE Tr %d, Si %d, Se %d -- ", track, side, sector);

        if(res != 0) {
            printf("FAIL -- res = %d\n", res);
            showBiosError(res);
        } else {
            printf("GOOD\n");
        }
    }
}

void showBiosError(int errorNo)
{
	switch(errorNo) {
		case 0:   printf("No error\n"); break;
		case -1:  printf("BIOS Error: Generic error\n"); break;
		case -2:  printf("BIOS Error: Drive not ready\n"); break;
		case -3:  printf("BIOS Error: Unknown command\n"); break;
		case -4:  printf("BIOS Error: CRC error\n"); break;
		case -5:  printf("BIOS Error: Bad request\n"); break;
		case -6:  printf("BIOS Error: Seek error\n"); break;
		case -7:  printf("BIOS Error: Unknown media\n"); break;
		case -8:  printf("BIOS Error: Sector not found\n"); break;
		case -9:  printf("BIOS Error: Out of paper\n"); break;
		case -10: printf("BIOS Error: Write fault\n"); break;
		case -11: printf("BIOS Error: Read fault\n"); break;
		case -12: printf("BIOS Error: Device is write protected\n"); break;
		case -14: printf("BIOS Error: Media change detected\n"); break;
		case -15: printf("BIOS Error: Unknown device\n"); break;
		case -16: printf("BIOS Error: Bad sectors on format\n"); break;
		case -17: printf("BIOS Error: Insert other disk (request)\n"); break;
		default:  printf("BIOS Error: something other...\n"); break;
	}
}


