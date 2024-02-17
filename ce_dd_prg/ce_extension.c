#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "acsi.h"
#include "global.h"
#include "hdd_if.h"
#include "ce_extension.h"

extern BYTE deviceID;

// call the CE extension based on supplied structure content
void ceExtensionCall(CEXcall *cc)
{
    cc->cmd = (deviceID << 5) | cc->cmd;												// add device ID to command
    uint8_t direction = (cc->readNotWrite == DATA_DIR_READ) ? ACSI_READ : ACSI_WRITE;	// data direction
    (*hdIf.cmd)(direction, &cc->cmd, 6, cc->buffer, cc->sectorCount);					// send command to host over ACSI

    if(!hdIf.success) {     // on failure - store failure as status byte
        cc->statusByte = STATUS_NO_RESPONSE;
    } else {                // on success - store received status byte
        cc->statusByte = hdIf.statusByte;
    }
}
