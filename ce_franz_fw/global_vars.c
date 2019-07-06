#include "defs.h"
#include "main.h"

TWriteBuffer wrBuffer[2];                           // two buffers for written sectors
TWriteBuffer *wrNow;

SStreamed streamed;

WORD mfmReadStreamBuffer[16];                           // 16 words - 16 mfm times. Half of buffer is 8 times - at least 32 us (8 * 4us),

WORD mfmWriteStreamBuffer[16];

WORD version[2] = {0xf018, 0x1205};             // this means: Franz, 2018-12-05
WORD drive_select;

volatile BYTE sendFwVersion, sendTrackRequest;

WORD atnSendFwVersion       [ATN_SENDFWVERSION_LEN_TX];
WORD cmdBuffer              [CMD_BUFFER_SIZE];
WORD atnSendTrackRequest    [ATN_SENDTRACK_REQ_LEN_TX];
WORD readTrackDataBfr       [READTRACKDATA_SIZE_WORDS];
BYTE *readTrackData;
WORD inIndexGet;

WORD fakeBuffer;

volatile WORD prevIntTime;

volatile BYTE spiDmaIsIdle;
volatile BYTE spiDmaTXidle, spiDmaRXidle;       // flags set when the SPI DMA TX or RX is idle

volatile TDrivePosition now, next, lastRequested, prev;
volatile WORD lastRequestTime;

BYTE hostIsUp;                                  // used to just pass through IKBD until RPi is up

BYTE driveId;
BYTE driveEnabled;

BYTE isDiskChanged;
BYTE isWriteProtected;

WORD trackStreamedCount = 0;

volatile TCircBuffer buff0, buff1;

TOutputFlags outFlags;

BYTE sectorsWritten;            // how many sectors were written during the last media rotation - if something was written, we need to get the re-encoded track
