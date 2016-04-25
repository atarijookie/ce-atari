#include "mfmdecoder.h"
#include "../debug.h"

//#define LOGTOFILE

#ifdef LOGTOFILE
FILE *flog;
#endif

MfmDecoder::MfmDecoder()
{
    patCount = 0;
    bitCount = 0;

    byte = 0;
}

void MfmDecoder::decodeStream(BYTE *inStream, int inCount, BYTE *outData, int &outCount)
{
    decoded = outData;
    index   = 0;

    a1Cnt = 0;
    c2Cnt = 0;

#ifdef LOGTOFILE
    flog = fopen("C:\\floppy_log.bin", "wb");
#endif

    for(int i=0; i<inCount; i++) {              // process all the in stream data
        if(inStream[i] == 0) {                  // skip empty byte
            continue;
        }

        if(inStream[i] == 0x50) {               // if it's CMD_CURRENT_SECTOR command, skip it
            i += 3;
            continue;                           // skip this command
        }

        appendEncodedByte(inStream[i]);
    }

    Debug::out(LOG_DEBUG, "A1 marks found: %d", a1Cnt);
    Debug::out(LOG_DEBUG, "C2 marks found: %d", c2Cnt);

#ifdef LOGTOFILE
    fclose(flog);
#endif

    outCount = index;                           // and store the count of data decoded
}

void MfmDecoder::appendEncodedByte(BYTE val)
{
    for(int i=0; i<4; i++) {
        BYTE time = val >> 6;                   // get highest 2 bits
        val = val << 2;

        switch(time) {
        case MFM_4US:                           // append '10'
            appendPatternBit(1);
            appendPatternBit(0);

            appendToSync(4);
            break;

        case MFM_6US:                           // append '100'
            appendPatternBit(1);
            appendPatternBit(0);
            appendPatternBit(0);

            appendToSync(6);
            break;

        case MFM_8US:                           // append '1000'
            appendPatternBit(1);
            appendPatternBit(0);
            appendPatternBit(0);
            appendPatternBit(0);

            appendToSync(8);
            break;

        default: 
            Debug::out(LOG_ERROR, "appendEncodedByte -- something is wrong...");
            break;
        }
    }
}

bool MfmDecoder::appendToSync(WORD val)
{
    static DWORD syncDetector = 0;

    syncDetector = (syncDetector << 4) | val;

    if((syncDetector & 0xfffff) == 0x46866) {          // C2
        c2Cnt++;
    }

    if((syncDetector & 0xffff) == 0x8686) {            // A1
        syncDetector = 0;

        a1Cnt++;

//        qDebug() << "A1 at -- bitCount: " << bitCount << ", patCount: " << patCount << ", index: " << index;

        bitCount = 7;
        patCount = 1;

#ifdef LOGTOFILE
        fputc(0xa1, flog);
#endif

        return true;
    }

    return false;
}

void MfmDecoder::appendPatternBit(BYTE bit)
{
    static BYTE pattern = 0;

    pattern = (pattern << 1) | bit;         // add this bit
    pattern = pattern & 0x03;               // leave only 2 lowest bits
    patCount++;

    if(patCount == 2) {                     // got 2 bits / flux reversals
        patCount = 0;

        switch(pattern) {                   // now append the right normal bit
        case 2:     addNormalBit(0); break;
        case 0:     addNormalBit(0); break;
        case 1:     addNormalBit(1); break;
        default:    addNormalBit(0); break;
        }
    }
}

void MfmDecoder::addNormalBit(BYTE bit)
{
    byte = (byte << 1) | bit;               // append this bit
    bitCount++;

    if(bitCount == 8) {                     // if got the whole byte, store it
        bitCount = 0;

#ifdef LOGTOFILE
        fputc(' ', flog);
#endif

//        if(byte == 0xfe) {
//            qDebug() << "FE now!   index: " << index;
//        }

        decoded[index] = byte;
        index++;
    }
}
