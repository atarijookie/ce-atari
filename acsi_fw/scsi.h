#ifndef _SCSI_DEFS_H_
#define _SCSI_DEFS_H_

// commands with length 6 bytes
#define SCSI_C_WRITE6                           0x0a
#define SCSI_C_READ6                            0x08

// commands with length 10 bytes
#define SCSI_C_WRITE10                          0x2a
#define SCSI_C_READ10                           0x28

void processScsiLocaly(void);

#endif
