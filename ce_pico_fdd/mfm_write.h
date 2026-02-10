#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "defs.h"

// exact expected pulses would be: 23 ticks, 43 ticks, 63 ticks
#define PULSE_TOO_SHORT 13
#define PULSE_4US       33
#define PULSE_6US       53
#define PULSE_8US       73

extern queue_t fifoMfmWrite;
extern TWriteBuffer wrBuffer;  // buffer for written sectors

void pio_mfm_write_setup(void);
