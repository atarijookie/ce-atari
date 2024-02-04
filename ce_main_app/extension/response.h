#ifndef RESPONSE_H
#define RESPONSE_H

#include <string>
#include <stdint.h>
#include "extensiondefs.h"

// marks the response as empty / not received / received
#define RESP_STATE_EMPTY        0
#define RESP_STATE_NOT_RECEIVED 1
#define RESP_STATE_RECEIVED     2

class Response
{
public:
    void clear(void);
    uint8_t updateBeforeSend(void);
    void store(uint8_t statusByte, uint8_t* data, uint32_t dataLen);

    uint8_t funcCallId;             // autoincrement id, used as function call handle
    uint8_t state;                  // one of the RESP_STATE_* values
    uint8_t statusByte;             // status byte that should be returned for this response
    uint8_t data[EXT_BUFFER_SIZE];  // data that should be returned for this response
    uint32_t dataLen;               // count of bytes in data buffer
};

#endif
