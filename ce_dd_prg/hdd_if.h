#ifndef _HDD_IF_H_
#define _HDD_IF_H_

#define HDD_IF_ACSI         0
#define HDD_IF_SCSI_TT      1
#define HDD_IF_SCSI_FALCON  2

BYTE hdd_if_cmd(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount);

#endif
