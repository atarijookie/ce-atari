#ifndef _GLOBAL_VARS_H_
#define _GLOBAL_VARS_H_

#include "defs.h"

extern TWriteBuffer wrBuffer[2];                           // two buffers for written sectors
extern TWriteBuffer *wrNow;

extern SStreamed streamed;

extern WORD mfmReadStreamBuffer[16];                           // 16 words - 16 mfm times. Half of buffer is 8 times - at least 32 us (8 * 4us),

#define MFM_WRITE_STREAM_SIZE           64
#define MFM_WRITE_STREAM_SIZE_HALF      (MFM_WRITE_STREAM_SIZE / 2)
extern WORD mfmWriteStreamBuffer[MFM_WRITE_STREAM_SIZE];

extern WORD version[2];
extern WORD drive_select;

extern volatile BYTE sendFwVersion, sendTrackRequest;

extern DWORD c1;
extern WORD atnSendFwVersion       [ATN_SENDFWVERSION_LEN_TX];
extern DWORD c2;
extern WORD cmdBuffer              [CMD_BUFFER_SIZE];
extern DWORD c3;
extern WORD atnSendTrackRequest    [ATN_SENDTRACK_REQ_LEN_TX];
extern DWORD c4;
extern WORD readTrackDataBfr       [READTRACKDATA_SIZE_WORDS];
extern DWORD c5;
extern BYTE *readTrackData;
extern WORD inIndexGet;

extern WORD fakeBuffer;

extern volatile WORD prevIntTime;

extern volatile BYTE spiDmaIsIdle;
extern volatile BYTE spiDmaTXidle, spiDmaRXidle;       // flags set when the SPI DMA TX or RX is idle

extern volatile TDrivePosition now, next, lastRequested, prev;
extern volatile WORD lastRequestTime;

extern BYTE hostIsUp;                                  // used to just pass through IKBD until RPi is up

extern BYTE driveId;
extern BYTE driveEnabled;

extern BYTE isDiskChanged;
extern BYTE isWriteProtected;

extern WORD trackStreamedCount;

extern TCircBuffer buff0, buff1;

extern TOutputFlags outFlags;

extern BYTE sectorsWritten;            // how many sectors were written during the last media rotation - if something was written, we need to get the re-encoded track

#endif
