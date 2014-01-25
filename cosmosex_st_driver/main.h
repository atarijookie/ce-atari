#ifndef _MAIN_H_
#define _MAIN_H_


#define DMA_BUFFER_SIZE		512

#define Clear_home()    Cconws("\33E")

#define DEVICEID_GET	0xff
BYTE getSetDeviceId(BYTE newId);


#endif