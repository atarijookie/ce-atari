//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <mint/linea.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"

#define BUFFER_SIZE (512 + 2)       // extra 2 bytes, because pBuffer will be aligned to even address
uint8_t myBuffer[BUFFER_SIZE];
uint8_t *pBuffer;

//--------------------------------------------------
int main(void)
{
    // create buffer pointer to even address 
    uint32_t toEven;
    toEven = (uint32_t) &myBuffer[0];
  
    if(toEven & 0x0001)         // not even number? 
        toEven++;
  
    pBuffer = (uint8_t*) toEven; 
    
    Clear_home();

    

    return 0;
}
