#ifndef __UTILS_H__
#define __UTILS_H__

uint16_t getWord(uint8_t *bfr);
uint32_t getDword(uint8_t *bfr);
uint32_t get24bits(uint8_t *bfr);
void storeWord(uint8_t *bfr, uint16_t val);
void storeDword(uint8_t *bfr, uint32_t val);
void store24bits(uint8_t *bfr, uint32_t val);

#endif
