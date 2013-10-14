//--------------------------------------------
#include "mydefines.h"
//--------------------------------------------
void SetUpCmd(void);
void DumpBuffer(void);
BYTE IsCardInserted(BYTE which);
void fputD(WORD inp, BYTE *buffer);
void DumpClockRegs(void);
void ShowLastResetReason(void);
void copyn(BYTE *dest, BYTE *src, WORD ncount);
BYTE cmpn(BYTE *one, BYTE *two, WORD len);
BYTE HexToByte(BYTE a, BYTE b);
void memset(BYTE *what, BYTE value, WORD count);

extern void SetupClocks(void);

void wait_ms(DWORD ms);
void wait_us(DWORD us);

void Timer0_start(void);

BYTE TimeOut_DidHappen(void);
void TimeOut_Stop(void);
void TimeOut_Start_ms(DWORD ms);
void TimeOut_Start_us(DWORD us);

BYTE Config_Read(BYTE DontInit);
void Config_SetDefault(void);
BYTE Config_GetBootBase(void);

void ReadInquiryName(void);

void InitStorage(void);
void ListStorage(void);
void ClearAllStorage(void);
//--------------------------------------------
