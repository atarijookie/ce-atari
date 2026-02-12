#ifndef __PSRAM_H__
#define __PSRAM_H__

void psramWriteBuffer(uint32_t addr, const uint8_t *buffer, size_t length);
void psramReadBuffer(uint32_t addr, uint8_t *buffer, size_t length);
void psramTest(void);

void psramStoreTrack(int track, int side, uint8_t* data);
void psramLoadTrack(int track, int side, uint8_t* data);
void psramLoadTrack_currentSectorFirst(int track, int side, uint8_t* trackDataStart);

void psramStoreSector(int track, int side, int byteOffsetFromTrackStart, uint8_t* data, uint32_t copyLength, uint32_t clearLength);
void psramLoadSector(int track, int side, int sector, uint8_t* trackDataStart);

void psramConfigFlagSet(void);
void psramConfigFlagClear(void);
bool psramConfigFlagGet(void);

void loadSettingsFromPSRAM(void);
void saveSettingsToPSRAM(void);

#endif
