#ifndef _HELPERS_H_
#define _HELPERS_H_

BYTE *getDmaBufferPointer(void);

BYTE getCurrentDrive(void);
void setCurrentDrive(BYTE newDrv);

WORD getCeDrives(void);

#define PDTA_GET			((BYTE *) 0xffffffff)
BYTE *getSetPDta(BYTE *newVal);

#define GET_CURRENTDRIVE	0xff
BYTE getSetCurrentDrive(BYTE newVal);

#endif

