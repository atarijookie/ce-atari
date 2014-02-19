#include <cr_section_macros.h>
#include "system_LPC8xx.h"
#include "lpc8xx_uart.h"

int main(void) {
	SystemInit();
	SystemCoreClockUpdate();



    while(1);

    return 0 ;
}
