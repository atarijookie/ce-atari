#define STM32F10X_LD
#include <stm32f10x.h>                       /* STM32F103 definitions         */

int main (void) 
{
  unsigned int i;                            /* LED variable                  */

  RCC->APB2ENR |= (1UL << 3);                /* Enable GPIOB clock            */

  GPIOB->CRH    =  0x33333333;               /* PB.8..16 defined as Outputs   */

  while (1)  {                               /* Loop forever                  */
    for (i = 1<<8; i < 1<<15; i <<= 1) {     /* Blink LED 0,1,2,3,4,5,6       */
      GPIOB->BSRR = i;                       /* Turn LED on                   */

      GPIOB->BRR = i;                        /* Turn LED off                  */
    }
    for (i = 1<<15; i > 1<<8; i >>=1 ) {     /* Blink LED 7,6,5,4,3,2,1       */
      GPIOB->BSRR = i;                       /* Turn LED on                   */

      GPIOB->BRR = i;                        /* Turn LED off                  */
    }
  }
}
