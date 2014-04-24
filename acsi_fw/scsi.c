#include "defs.h"
#include "scsi.h"

extern BYTE cmd[14];									// received command bytes
extern BYTE cmdLen;										// length of received command
extern BYTE brStat;										// status from bridge

void processScsiLocaly(BYTE justCmd)
{
    DWORD sector;
    WORD lenX;
    
    // if it's 6 byte RW command
    if(justCmd == SCSI_C_WRITE6 || justCmd == SCSI_C_READ6) {
        sector  = (cmd[1] & 0x1f);      // get the starting sector
        sector  = sector << 8;
        sector |= cmd[2];
        sector  = sector << 8;
        sector |= cmd[3];

        lenX = cmd[4];	   	 	   	    // get the # of sectors to read
    }

    // if it's 10 byte RW command
    if(justCmd == SCSI_C_WRITE10 || justCmd == SCSI_C_READ10) {
        sector  = cmd[3];               // get the starting sector
        sector  = sector << 8;
        sector |= cmd[4];
        sector  = sector << 8;
        sector |= cmd[5];
        sector  = sector << 8;
        sector |= cmd[6];

        lenX  = cmd[8];	  	   		    // get the # of sectors to read
        lenX  = lenX << 8;
        lenX |= cmd[9];
    }
    
    // for sector read commands
    if(justCmd == SCSI_C_READ6 || justCmd == SCSI_C_READ10) {
        
        return;
    }
    
    // for sector write commands
    if(justCmd == SCSI_C_WRITE6 || justCmd == SCSI_C_WRITE6) {
        
        return;
    }
}

