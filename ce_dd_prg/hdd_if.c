
#include "acsi.h"
#include "hdd_if.h"

BYTE whichHddIf;    

BYTE acsi_cmd       (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
BYTE scsi_cmd_TT    (BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);
BYTE scsi_cmd_Falcon(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

BYTE hdd_if_cmd(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
    BYTE res;
    
    switch(whichHddIf) {
        case HDD_IF_SCSI_TT    : res = scsi_cmd_TT    (readNotWrite, cmd, cmdLength, buffer, sectorCount);  break;
        
        case HDD_IF_SCSI_FALCON: res = scsi_cmd_Falcon(readNotWrite, cmd, cmdLength, buffer, sectorCount);  break;

        case HDD_IF_ACSI       :
        default                : res = acsi_cmd       (readNotWrite, cmd, cmdLength, buffer, sectorCount);  break;
    }
    
    return res;
}

