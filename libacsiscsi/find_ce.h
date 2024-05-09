#ifndef _FIND_CE_H_
#define _FIND_CE_H_

#define FIND_DEV_CE     1
#define FIND_DEV_CS     2

#define MACHINE_ST      0
#define MACHINE_TT      2
#define MACHINE_FALCON  3

#define DEVICE_NOT_FOUND    0xff

uint8_t findCE(uint8_t device, uint8_t hddIf);
uint8_t ce_identify(uint8_t id, uint8_t hddIf);
uint8_t cs_inquiry(uint8_t id, uint8_t hddIf);
uint8_t getMachineType(void);
uint8_t findDevice(uint8_t whatDev);
uint8_t findDeviceInSupervisor(void);

#endif
