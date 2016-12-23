#include "defs.h"
#include "scsi.h"
#include "cosmosolo.h"
#include "bridge.h"
#include "mmc.h"
#include "eeprom.h"

extern BYTE cmd[14];            // received command bytes
extern BYTE cmdLen;             // length of received command
extern BYTE brStat;             // status from bridge

extern TDevice sdCard;
extern BYTE sdCardID;
extern char *VERSION_STRING_SHORT;
extern char *DATE_STRING;

extern BYTE firstConfigReceived;

extern BYTE enabledIDs[8];      // when 1, Hanz will react on that ACSI ID #
extern BYTE sdCardID;

void updateEnabledIDsInSoloMode(void);

void processCosmoSoloCommands(BYTE isIcd)
{
    BYTE good = FALSE;

    if(isIcd) {                 // it's ICD command (long)?
        switch(cmd[5]) {        // the 5th byte is the command byte
            case TEST_READ:         good = soloTestReadNotWrite(1);    break;
            case TEST_WRITE:        good = soloTestReadNotWrite(0);    break;
            case TEST_GET_ACSI_IDS: good = soloTestGetACSIids();       break;
            default:                break;
        }
    } else {                    // it's not ICD command (short)?
        good = cosmoSoloSetNewId();
    }

    if(good) {                  // good?
        scsi_sendOKstatus();
    } else {                    // bad?
        PIO_read(SCSI_ST_CHECK_CONDITION);
    }
}

BYTE cosmoSoloSetNewId(void)
{
    BYTE good = FALSE;

    if(cmd[3] == sdCardID) {            // if the ID we want to change matches our sdCardID
        BYTE newId = cmd[4];

        if(newId <= 7) {                // new ID must be between 0 and 7
            sdCardID = newId;
            EE_WriteVariable(EEPROM_OFFSET_SDCARD_ID, sdCardID);    // store the new SD card ID to fake EEPROM

            updateEnabledIDsInSoloMode();

            good = TRUE;                // mark success
        }
    }

    return good;                        // return if good or not
}

BYTE soloTestReadNotWrite(BYTE readNotWrite)
{
    BYTE good = TRUE;
    DWORD i, byteCount;
    WORD counter = 0;
    WORD xorVal;

    // how many bytes?
    byteCount  = cmd[6];
    byteCount  = byteCount << 8;
    byteCount |= cmd[7];
    byteCount  = byteCount << 8;
    byteCount |= cmd[8];

    // what XOR value?
    xorVal     = cmd[9];
    xorVal     = xorVal << 8;
    xorVal    |= cmd[10];

    for(i=0; i<byteCount; i += 2) {
        WORD expectedData = counter ^ xorVal;       // create word, use it in hiByte-loByte order

        if(readNotWrite) {                          // read? (to ST)
            DMA_read((BYTE) (expectedData >> 8));   // send hi byte

            if(brStat != E_OK) {                    // hi byte failed? fail, quit
                good = FALSE;
                break;
            }

            DMA_read((BYTE) expectedData);          // send lo byte

            if(brStat != E_OK) {                    // lo byte failed? fail, quit
                good = FALSE;
                break;
            }
        } else {                                    // write? (to device)
            BYTE loByte, hiByte;
            WORD realWord;

            hiByte = DMA_write();                   // get hi byte

            if(brStat != E_OK) {                    // hi byte failed? fail, quit
                good = FALSE;
                break;
            }

            loByte = DMA_write();                   // get lo byte

            if(brStat != E_OK) {                    // lo byte failed? fail, quit
                good = FALSE;
                break;
            }

            realWord = (((WORD) hiByte) << 8) | ((WORD) loByte);   // construct real received WORD

            if(realWord != expectedData) {          // data mismatch? mark fail, but keep getting rest of data
                good = FALSE;
            }
        }

        counter++;
    }

    return good;                        // return if good or not
}

BYTE soloTestGetACSIids(void)
{
    BYTE enabledIdsBits = 0;
    BYTE id=0;
    BYTE i;

    // bytes 0..7
    for(id=0; id<8; id++) {                             // now store it one after another to buffer
        BYTE fakeDevType;

        if(enabledIDs[id] != 0) {                       // device enabled?
            enabledIdsBits |= (1 << id);                // set bit if enabled

            if(id == sdCardID) {                        // it's enabled and it's SD card? it's SD card
                fakeDevType = DEVTYPE_SD;
            } else {                                    // it's enabled and it's not SD card? let's pretend it's CE_DD
                fakeDevType = DEVTYPE_TRANSLATED;
            }
        } else {                                        // device disabled?
            fakeDevType = DEVTYPE_OFF;
        }


        DMA_read(fakeDevType);                          // return the (fake) device type
    }

    // byte 8
    DMA_read(enabledIdsBits);                           // store the enabled ACSI IDs

    // bytes 9 and 10
    DMA_read(sdCardID);                                 // store the SD card ID
    DMA_read(sdCard.IsInit);                            // store if the SD card is present and initialized

    // we previously sent too few bytes to ST, so now we need to fill it up to 16 bytes so we can safely get it from DMA chip into ST
    for(i=0; i<5; i++) {                                // pad to multiple of 16
        DMA_read(0);
    }

    return TRUE;
}

