#include <stdio.h>
#include <stdint.h>

#define BYTE	unsigned char
#define WORD	unsigned short
#define DWORD	unsigned int

WORD calcChecksum(char *filename);

int main(int argc, char *argv[])
{
	if(argc < 2 || argc > 3) {
		printf("\nUsage: checksumgen filename [0xChecksum]\n");
		return 0;
	}
	
	if(argc == 2) {
		char *filename = argv[1];
		WORD cscalc = calcChecksum(filename);
		printf("\nChecksum - calculated: 0x%04x\n", (int) cscalc);
		return 0;
	}
	
	if(argc == 3) {
		char *filename = argv[1];
		char *cs = argv[2];
		int csin;
		
		int ires = sscanf(cs, "0x%x", &csin);
		
		if(ires != 1) {
			printf("\nFailed to read provided checksum!\n");
			return 0;
		}
		
		WORD cscalc = calcChecksum(filename);
		printf("\nChecksum - provided: 0x%04x, calculated: 0x%04x\n", csin, (int) cscalc);
		
		if(cscalc == csin) {
			printf("Checksum is OK\n");
		} else {
			printf("Checksum is BAD\n");
		}
		
		return 0;
	}

	return 0;
}

WORD calcChecksum(char *filename)
{
    FILE *f = fopen(filename, "rb");

    if(!f) {
        printf("File %s -- failed to open file, checksum failed", filename);
        return 0;
    }

    WORD cs = 0;
    WORD val, val2;

    while(!feof(f)) {                       // for whole file
        val = (BYTE) fgetc(f);              // get upper byte
        val = val << 8;

        if(!feof(f)) {                      // if not end of file
            val2 = (BYTE) fgetc(f);         // read lowe byte from file
        } else {
            val2 = 0;                       // if end of file, put a 0 there
        }

        val = val | val2;                   // create a word out of it

        cs += val;                          // add it to checksum
    }

    fclose(f);
    return cs;                // return if the calculated cs is equal to the provided cs
}
