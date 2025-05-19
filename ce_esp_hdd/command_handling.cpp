#include "WiFi.h"

#include "defs.h"
#include "bridge.h"
#include "utils.h"
#include "command_handling.h"
#include "connection.h"
#include "scsi.h"

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

void onGetCommand(void)
{
    uint8_t i, id;

    //---------
    // retrieve the command. There are some slight differences between ACSI and SCSI part,
    // but the resulting commands should be the same (to make the rest of app work without further changes).
    uint8_t good;

    if (isAcsiNotScsi)
    { // for ACSI
        good = onGetCommandAcsi();
    }
    else
    { // for SCSI
        good = onGetCommandScsi();
    }

    if (!good)
    { // if failed to get the cmd, quit
        return;
    }

    id = (cmd[0] >> 5) & 0x07; // get only device ID

    //-----
    if (!idIsEnabled(id)) // this ID not enabled, ignore command
    {
        return;
    }

    //----------------
    // if we got here, we should handle this in host
    timeoutStart(); // start the timeout timer to give the rest of code full timeout time

    sendHeaderAndDataToHost(SOCK_HDD, atnSendACSIcommand, ATN_SENDACSICOMMAND_LEN_TX - TX_HEADER_SIZE);

    state = STATE_WAIT_COMMAND_RESPONSE;
}

uint8_t onGetCommandAcsi(void)
{
    uint8_t id, i;

    //----------------------
    cmd[0] = PIO_writeFirst(); // get byte from ST (waiting for the 1st byte)
    id = (cmd[0] >> 5) & 0x07; // get only device ID

    // Serial.print("onGetCommandAcsi - cmd[0]: ");
    // Serial.print(cmd[0], HEX);
    // Serial.print(", id: ");
    // Serial.print(id);

    //----------------------
    if (!idIsEnabled(id)) // if this ID is not enabled, quit
    {
        resetBridge();
        timeoutClear();
        // Serial.println(" NOT ENABLED");
        return 0;
    }

    cmdLen = 6; // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()

    for (i = 1; i < cmdLen; i++)
    {                         // receive the next command bytes
        cmd[i] = PIO_write(); // drop down IRQ, get byte

        if (brStat != E_OK)
        { // if something was wrong, quit, failed
            // Serial.print(" failed on cmd #");
            // Serial.println(i);
            timeoutClear();
            resetBridge();
            return 0;
        }

        if (i == 1)
        {                                   // if we got also the 2nd byte
            getCmdLengthFromCmdBytesAcsi(); // we set up the length of command, etc.
        }
    }

    // for(i=1; i<cmdLen; i++) {
    //     Serial.print(" ");
    //     Serial.print(cmd[i], HEX);
    // }
    // Serial.println("");

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

    if (id == 0xff)
    { // ID not found? quit
        return 0;
    }
    //----------------------
    if (!idIsEnabled(id)) // if this ID is not enabled, quit
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

void onDataRead(uint8_t withStatus)
{
    state = STATE_GET_COMMAND; // this will be the next state once this function finishes with fail

    // nothing to send AND should send status? then just quit with status byte
    if (dataCnt == 0 && withStatus)
    {
        PIO_read(statusByte);
        return;
    }

    uint32_t start = millis();
    while(true)
    {
        handleIncommingData();      // receive data and wait for dataReceived flag

        if((millis() - start) > 1000) {
            PIO_read(SCSI_ST_CHECK_CONDITION);
            return;
        }

        if(dataReceived) {
            break;
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

        // if (hasTimedOut)
        // {                               // if the data from host doesn't come within timeout, quit
        //     setDataDirection(DIR_RECV); // data direction for writing, and quit
        //     return;
        // }

        for(int i=0; i<cntNow; i++) {
            DMA_read(data[i]);

            if (brStat == E_TimeOut)
            {
                setDataDirection(DIR_RECV); // data direction for writing, and quit
                return;
            }
        }
    }

    if (withStatus)     // if should send status, then send status and go to STATE_GET_COMMAND
    {
        state = STATE_GET_COMMAND;
        PIO_read(statusByte);   // send the status to Atari
    }
    else                // if shouldn't send status here, switch to state STATE_READ_STATUS, which will retrieve status from host and send it to ST
    {
        state = STATE_READ_STATUS;
    }
}

void onDataWrite(void)
{
    // create and send one header at the start
    uint8_t header[TX_HEADER_SIZE];

    storeHeader(header, ATN_WRITE_MORE_DATA, dataCnt);
    sendDataToHost(SOCK_HDD, header, TX_HEADER_SIZE);

    // get data from Atari and send it to host by sector sized chunks
    uint8_t data[512];
    setDataDirection(DIR_RECV);     // data direction for reading

    while (dataCnt > 0)             // something to write?
    {
        uint32_t cntNow = MIN(dataCnt, 512);
        dataCnt -= cntNow;

        for(int i = 0; i < cntNow; i++)
        {
            data[i] = DMA_write();          // get data from Atari

            if (brStat == E_TimeOut)
            {                              // if timeout occured
                state = STATE_GET_COMMAND; // transfer failed, don't send status, just get next command
                return;
            }
        }

        sendDataToHost(SOCK_HDD, data, cntNow);     // send to host
    }

    state = STATE_WAIT_FOR_STATUS_ARRIVAL;  // continue with sending the status
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
