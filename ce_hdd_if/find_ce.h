#ifndef _FIND_CE_H_
#define _FIND_CE_H_

#define MACHINE_ST      0
#define MACHINE_TT      2
#define MACHINE_FALCON  3

// whichIF -- which IF we want to specifically search through; can be: IF_ACSI, IF_SCSI_TT, IF_SCSI_FALCON, IF_CART. If zero, defaults to all IFs available on this machine.
// whichDevType -- which dev type we want to find DEV_CE, DEV_CS, DEV_OTHER. If zero, defaults to DEV_CE
// If you use findDevice(0, 0), if will look for CE only on all available buses on current machine.
BYTE findDevice(BYTE whichIF, BYTE whichDevType);

// return found device type which was found when findDevice() did run last time
BYTE getDevTypeFound(void);

BYTE getMachineType(void);

#endif
