#include "WiFi.h"

#include "defs.h"
#include "bridge.h"
#include "utils.h"
#include "command_handling.h"
#include "connection.h"
#include "scsi.h"
#include "rw_tasks.h"

extern NetworkClient clientHdd;
extern NetworkClient clientIkbd;

void onButtonPress(void);

uint8_t onGetCommandAcsi(void);
uint8_t onGetCommandScsi(void);
void getCmdLengthFromCmdBytesAcsi(void);
void getCmdLengthFromCmdBytesScsi(uint8_t cmd);

extern uint8_t state;
extern uint32_t dataCnt;
extern uint8_t statusByte;

extern uint8_t atnSendACSIcommand[ATN_SENDACSICOMMAND_LEN_TX];

//----------
extern uint8_t *cmd;   // received command bytes, should point beyond the header in atnSendACSIcommand
extern uint8_t cmdLen; // length of received command
extern uint8_t brStat; // status from bridge
extern uint8_t lastScsiStatusByte;

extern uint8_t enabledIDs;

extern uint8_t isAcsiNotScsi;
extern uint8_t busIdle;
extern bool dataReceived;

uint8_t onGetCommand(void)
{
    //---------
    // retrieve the command. There are some slight differences between ACSI and SCSI part,
    // but the resulting commands should be the same (to make the rest of app work without further changes).
    uint8_t good = isAcsiNotScsi ? onGetCommandAcsi() : onGetCommandScsi();

    if (!good)  // if failed to get the cmd, quit
    {
        timeoutClear();
        return STATE_GET_COMMAND;
    }

    //----------------
    // command received, send it to host
    timeoutStart(); // start the timeout timer to give the rest of code full timeout time

    sendHeaderAndDataToHost(SOCK_HDD, atnSendACSIcommand, ATN_SENDACSICOMMAND_LEN_TX - TX_HEADER_SIZE);

    return STATE_WAIT_COMMAND_RESPONSE;
}

uint8_t onGetCommandAcsi(void)
{
    uint8_t id, i;

    //----------------------
    cmd[0] = PIO_writeFirst(); // get byte from ST (waiting for the 1st byte)
    id = (cmd[0] >> 5) & 0x07; // get only device ID

#ifdef LOG_MORE
    Serial.print("\n\nonGetCommandAcsi - cmd[0]: ");
    Serial.print(cmd[0], HEX);
    Serial.print(", id: ");
    Serial.println(id);
#endif

    //----------------------
    if (!idIsEnabled(id)) // if this ID is not enabled, quit
    {
        resetBridge();
        // Serial.println(" NOT ENABLED");
        return 0;
    }

    cmdLen = 6; // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()

    for (i = 1; i < cmdLen; i++)
    {                         // receive the next command bytes
        cmd[i] = PIO_write(); // drop down IRQ, get byte

        if (brStat != E_OK)     // if something was wrong, quit, failed
        {
#ifdef LOG_MORE
            Serial.print(" failed on cmd #");
            Serial.println(i);
#endif
            resetBridge();
            return 0;
        }

        if (i == 1)
        {                                   // if we got also the 2nd byte
            getCmdLengthFromCmdBytesAcsi(); // we set up the length of command, etc.
        }
    }

#ifdef LOG_MORE
    for(i=1; i<cmdLen; i++) {
        Serial.print(" ");
        Serial.print(cmd[i], HEX);
    }
    Serial.println("");
#endif

    return 1;
}

uint8_t onGetCommandScsi(void)
{
    uint8_t id;
    uint8_t sel;
    int i;

    //----------------------
    sel = PIO_writeFirst(); // get SELection byte
    id = 0xff;              // mark that ID hasn't been found yet

    for (i = 0; i < 8; i++)
    {
        if ((sel & (1 << i)) != 0)
        { // if bit is one, this ID is selected
            if (idIsEnabled(id))
            {           // if that ID is enabled
                id = i; // store this ID and quit loop
                break;
            }
        }
    }

    if (id == 0xff || !idIsEnabled(id))     // id not found or id not enabled? quit
    {
        return 0;
    }

    cmdLen = 6; // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()

    for (i = 0; i < cmdLen; i++)
    {                         // receive the next command bytes
        cmd[i] = PIO_write(); // drop down IRQ, get byte

        if (brStat != E_OK)
        { // if something was wrong, quit, failed
            resetBridge();
            return 0;
        }

        if (i == 0)
        {                                         // if we got also the 2nd byte
            getCmdLengthFromCmdBytesScsi(cmd[0]); // we set up the length of command, etc.
        }
    }

    // now fix the command if the length is more than 6 bytes
    if (cmdLen > 6)
    {
        for (i = 13; i > 0; i--)
        { // move the cmd one byte further (to make cmd[0] unused)
            cmd[i] = cmd[i - 1];
        }
        cmd[0] = 0x1f; // store ICD command marker

        cmdLen++; // now the command is one byte longer
    }

    // for all commands add fake ACSI ID on top of the 0th byte
    cmd[0] = cmd[0] | (id << 5); // add ID on the top 3 bits
    return 1;
}

uint8_t onDataRead(uint8_t withStatus)
{
#ifdef LOG_MORE
    Serial.print("onDataRead withStatus ");
    Serial.print(withStatus);
    Serial.print(" dataCnt ");
    Serial.println(dataCnt);
#endif

    // nothing to send AND should send status? then just quit with status byte
    if (dataCnt == 0 && withStatus)
    {
        PIO_read(statusByte);
        return STATE_GET_COMMAND;   // next state: get next command
    }

    uint32_t start = millis();
    while(!dataReceived)
    {
        handleIncommingData();      // receive data and wait for dataReceived flag

        if(hasTimedOut) {
#ifdef LOG_MORE
    Serial.println("onDataRead timeout on wait for data received");
#endif
            PIO_read(SCSI_ST_CHECK_CONDITION);
            return STATE_GET_COMMAND;   // next state: get next command
        }
    }

    // now start the double buffered transfer to ST
    setDataDirection(DIR_SEND); // data direction for reading
    uint8_t data[512];

    while(dataCnt > 0)
    {
        uint32_t cntNow = MIN(512, dataCnt);
        dataCnt -= cntNow;

        clientHdd.read(data, cntNow);

        for(int i=0; i<cntNow; i++) {
            DMA_read(data[i]);

            if (brStat == E_TimeOut)
            {
#ifdef LOG_MORE
    Serial.println("onDataRead timeout on DMA_read");
#endif
                setDataDirection(DIR_RECV); // data direction for writing, and quit
                return STATE_GET_COMMAND;   // next state: get next command
            }
        }
    }

    // if should send status, then send status and go to STATE_GET_COMMAND
    if (withStatus)
    {
        PIO_read(statusByte);       // send the status to Atari
        return STATE_GET_COMMAND;   // next state: get next command
    }

    // if shouldn't send status here, switch to state STATE_READ_STATUS, which will retrieve status from host and send it to ST
    return STATE_READ_STATUS;       // next state: read status
}

uint8_t onDataWrite(void)
{
#ifdef LOG_MORE
    Serial.print("onDataWrite dataCnt ");
    Serial.println(dataCnt);
#endif

    // create and send one header at the start
    // uint8_t header[TX_HEADER_SIZE];
    // storeHeader(header, ATN_WRITE_MORE_DATA, dataCnt);
    // sendDataToHost(SOCK_HDD, header, TX_HEADER_SIZE);

    int idx = getEmptyBuffer();
    if(idx < 0) {
        Serial.println("onDataWrite failed to get empty buffer for header");
        return STATE_GET_COMMAND;
    }

    storeHeader(writeBuffers[idx].data, ATN_WRITE_MORE_DATA, dataCnt);  // store this ATN in a header
    submitBufferForWrite(idx, TX_HEADER_SIZE);  // this buffer can be written to socket

    // get data from Atari and send it to host by sector sized chunks
    // uint8_t data[512];
    setDataDirection(DIR_RECV);     // data direction for reading

    while (dataCnt > 0)             // something to write?
    {
        idx = getEmptyBuffer();
        if(idx < 0) {
            Serial.println("onDataWrite failed to get empty buffer for data");
            return STATE_GET_COMMAND;
        }
        uint8_t* pData = writeBuffers[idx].data;        // the data should be stored here before sending

        uint32_t cntNow = MIN(dataCnt, RW_BUFFER_SIZE);
        dataCnt -= cntNow;

        for(int i = 0; i < cntNow; i++)
        {
            pData[i] = DMA_write();          // get data from Atari

            if (brStat == E_TimeOut)
            {                              // if timeout occured
#ifdef LOG_MORE
    Serial.println("onDataWrite timeout on DMA_write");
#endif
                return STATE_GET_COMMAND; // transfer failed, don't send status, just get next command
            }
        }

        // sendDataToHost(SOCK_HDD, data, cntNow);     // send to host
        submitBufferForWrite(idx, cntNow);             // this buffer can be written to socket
    }

    return STATE_WAIT_FOR_STATUS_ARRIVAL;  // continue with sending the status
}

void onReadStatus(void)
{
    PIO_read(statusByte);       // send the status to Atari
}

void getCmdLengthFromCmdBytesAcsi(void)
{
    // now it's time to set up the receiver buffer and length
    if ((cmd[0] & 0x1f) == 0x1f)
    {                                 // if the command is '0x1f'
        switch ((cmd[1] & 0xe0) >> 5) // get the length of the command
        {
        case 0:
            cmdLen = 7;
            break;
        case 1:
            cmdLen = 11;
            break;
        case 2:
            cmdLen = 11;
            break;
        case 5:
            cmdLen = 13;
            break;
        default:
            cmdLen = 7;
            break;
        }
    }
    else
    {               // if it isn't a ICD command
        cmdLen = 6; // then length is 6 bytes
    }
}

void getCmdLengthFromCmdBytesScsi(uint8_t cmd)
{
    switch ((cmd & 0xe0) >> 5) // get the length of the command
    {
    case 0:
        cmdLen = 6;
        break;
    case 1:
        cmdLen = 10;
        break;
    case 2:
        cmdLen = 10;
        break;
    case 4:
        cmdLen = 16;
        break;
    case 5:
        cmdLen = 12;
        break;
    default:
        cmdLen = 6;
        break;
    }
}

uint8_t idIsEnabled(uint8_t id)
{
    if (id > 7)
    {
        return FALSE;
    }

    return (enabledIDs & (1 << id));
}
