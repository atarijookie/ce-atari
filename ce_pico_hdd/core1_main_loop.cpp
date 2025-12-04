#include <Ethernet.h>
#include <EEPROM.h>

#include "defs.h"
#include "bridge.h"
#include "utils.h"
#include "command_handling.h"
#include "connection.h"
#include "display.h"
#include "ikbd.h"
#include "ipc.h"

extern uint8_t cmd[16];  // received command bytes

uint8_t isAcsiNotScsi = 1;
uint8_t busIdle;

void core1_setup(void)
{
    flash_safe_execute_core_init();     // call this for flash_safe_execute() to work

    // config pins as inputs
    #define INPUTS_COUNT 13
    int inputs[INPUTS_COUNT] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7, PIN_CS, PIN_A1, PIN_ACK, PIN_RESET, PIN_SDA};

    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        pinMode(inputs[i], INPUT);
    }

    // config pins as outputs
    #define OUTPUTS_COUNT 4
    int outputs[OUTPUTS_COUNT] = {PIN_DATA_DIR, PIN_INT, PIN_DRQ, PIN_SCL};
    int levels[OUTPUTS_COUNT]  = {           0,       1,       1,       1};

    for (int i = 0; i < OUTPUTS_COUNT; i++)
    {
        gpio_set_function(outputs[i], GPIO_FUNC_SIO);
        gpio_set_dir(outputs[i], GPIO_OUT);
        gpio_put(outputs[i], levels[i]);
    }

    pioConfigAll();     // configure all PIO state machines

    resetBridge();
}

void core1_main_loop(void)
{
    core1_setup();

    uint32_t lastSendFwTime = millis();

    uint8_t state;
    state = STATE_GET_COMMAND;
    debug("starting cor1_main loop\n");

    while(1)
    {
        if(BIT_IS_L(PIN_RESET)) {   // when ACSI RESET is L, enter reset mode - no PIO transfers
            pioConfig(MODE_RESET);
        }

        // get the command from ACSI and send it to host
        // IN  STATE: STATE_GET_COMMAND
        // OUT STATE: WAIT_COMMAND_RESPONSE when GOOD, STATE_GET_COMMAND when FAIL
        if (state == STATE_GET_COMMAND)
        {
            if(PIO_gotFirstCmdByte())       // if 1st CMD byte was received
            {
                state = onGetCommand();
            }
            else
            {
                uint32_t now = millis();

                if ((now - lastSendFwTime) >= 1000)
                {
                    lastSendFwTime = now;

                    IPCbuffer* bfr = ipcGetFreeBuffer(0, CMD_TIMEOUT_SHORT);
                    if(bfr) {
                        ipcSetBufferAndPutToFifo(bfr, 0, STATE_SEND_FW_VER, 0, NULL, 0);
                    }
                }
            }
        }

        // something in the queue for core1? get it, handle it
        IPCbuffer* bfr = ipcGetBufferFromFifo(1);
        if(bfr) {
            // debug("c1: c %d\n", bfr->command);
            switch(bfr->command)
            {
                // read data to Atari
                case STATE_DATA_READ:
                {
                    timeoutStart(100);

                    bool res = onDataRead(bfr->length, bfr->data);

                    if(!res) {  // if failed, go to back to command receiving
                        state = STATE_GET_COMMAND;
                        timeoutClear();
                    }
                    break;
                }

                // write data from Atari to device
                case STATE_DATA_WRITE:
                {
                    uint32_t dataCnt = bfr->length;

                    longTimeout_basedOnSectorCount(dataCnt >> 9); // set timeout time based on how many sectors are transfered
                    bool res = onDataWrite(dataCnt);

                    if(!res) {  // if failed, go to back to command receiving
                        state = STATE_GET_COMMAND;
                        timeoutClear();
                    }
                    break;
                }

                // this happens after READ - send status byte to ST (read)
                case STATE_READ_STATUS:
                {
                    timeoutStart();             // start the timeout timer to give the rest of code full timeout time
                    uint8_t statusByte = bfr->data[0];
                    onReadStatus(statusByte);
                    state = STATE_GET_COMMAND;  // get the next command
                    timeoutClear();             // clear timeout, no need for it
                    break;
                }
            }

            bfr->free = true;
        }

        // if the data from host doesn't come within timeout, quit
        if (hasTimedOut)
        {
            timeoutClear();

#ifdef LOG_MORE
            debug("State : %d, cmd: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X, timeout at: %d\n",
                state, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9], cmd[10], cmd[11], millis());
#endif
            state = STATE_GET_COMMAND;
        }
    }
}
