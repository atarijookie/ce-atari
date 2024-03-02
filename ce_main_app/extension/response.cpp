#include <string.h>
#include <stdint.h>
#include "../utils.h"
#include "response.h"

void Response::clear(void)
{
    state = RESP_STATE_EMPTY;
    dataLen = 0;
}

uint8_t Response::updateBeforeSend(void)
{
    clear();                            // remove any remaining data
    state = RESP_STATE_NOT_RECEIVED;    // mark as not-received-response-yet
    funcCallId++;                       // increment function call id
    return funcCallId;                  // return updated function call id
}

void Response::store(uint8_t statusByte, uint8_t* data, uint32_t dataLen)
{
    state = RESP_STATE_RECEIVED;                // now received
    this->statusByte = statusByte;              // store status byte
    
    uint32_t storeDataLen = MIN(EXT_BUFFER_SIZE, dataLen);      // make sure to avoid buffer overflow
    memcpy(this->data, data, storeDataLen);     // copy in data
    this->dataLen = dataLen;                    // store data length
}
