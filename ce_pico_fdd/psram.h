#ifndef __PSRAM_H__
#define __PSRAM_H__

#include <arduino.h>

void psramWriteBuffer(uint32_t addr, const uint8_t *buffer, size_t length);
void psramReadBuffer(uint32_t addr, uint8_t *buffer, size_t length);
void psramTest(void);

void psramStoreTrack(int track, int side, uint8_t* data);
void psramLoadTrack(int track, int side, uint8_t* data);
void psramStoreSector(int track, int side, int byteOffsetFromTrackStart, uint8_t* data, uint32_t copyLength, uint32_t clearLength);

#endif
