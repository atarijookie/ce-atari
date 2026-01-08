
#include <cstdint>

#include "defs.h"
#include "display.h"
#include "utils.h"

void displayInit(void)
{
    debug("displayInit\n");

    gpio_set_function(PIN_SEL_IO_DP_DS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_RST_CD_REQ_CP, GPIO_FUNC_SIO);

    gpio_set_dir_out_masked((1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP));

    BIT_CLR(PIN_SEL_IO_DP_DS);
    BIT_CLR(PIN_RST_CD_REQ_CP);
}

// these codes are for segments order GFEDCBA
const uint8_t segmentsNumbers[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
const uint8_t segmentsLetters[26] = {0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x06, 0x1E, 0x00, 0x38, 0x00, 0x54, 0x3F, 0x73, 0x00, 0x50, 0x6D, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t segmentsSnake[8] = {0x01, 0x02, 0x40, 0x10, 0x08, 0x04, 0x40, 0x20};

// use this to transform standard .GFEDCBA (76543210) order to our ABC.DEGF (01273465) order
const uint8_t segmentOrder[8] = {0, 1, 2, 7, 3, 4, 6, 5};

extern volatile uint8_t core1state;

void display(uint8_t what)
{
    if(core1state != STATE_GET_COMMAND) {   // don't display anything unless the core1 is in the GET_COMMAND state (idle, between commands) - the display pins are shared with SCSI handshake
        return;
    }

    // change from SIO inputs to SIO outputs, so we can control the pins
    gpio_set_dir_out_masked((1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP));

    uint8_t val = 0;

    if(what >= '0' && what <= '9') {    // for numbers
        val = segmentsNumbers[what - '0'];
    }

    if(what >= 'A' && what <= 'Z') {    // for capital letters
        val = segmentsLetters[what - 'A'];
    }

    if(what >= 'a' && what <= 'z') {    // for small letters
        val = segmentsLetters[what - 'a'];
    }

    if(what >= DISP_SNAKE_0 && what <= DISP_SNAKE_7) {  // for progress snake
        val = segmentsSnake[what - DISP_SNAKE_0];
    }

    for(int i = 7; i >= 0; i--) {
        int bitNo = segmentOrder[i];    // .GFEDCBA to ABC.DEGF order

        if(val & (bitNo << 7)) {        // segment on? pin off
            BIT_CLR(PIN_SEL_IO_DP_DS);
        } else {                        // segment off? pin on
            BIT_SET(PIN_SEL_IO_DP_DS);
        }

        BIT_CLR(PIN_RST_CD_REQ_CP);    // CP to L
        busy_wait_at_least_cycles(10);
        BIT_SET(PIN_RST_CD_REQ_CP);    // CP to H
        busy_wait_at_least_cycles(10);
        BIT_SET(PIN_RST_CD_REQ_CP);    // CP to L
    }

    // in GET_COMMAND mode (selection) the display pins are used as SIO inputs
    gpio_set_dir_in_masked((1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP));
}
