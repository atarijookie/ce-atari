
#include "stm32f10x_flash.h"
#include "eeprom.h"
#include "defs.h"

WORD EE_ReadVariable(WORD offset, WORD defVal)
{
    WORD PageStatus0 = (*(__IO uint16_t*) PAGE0_BASE_ADDRESS);
    __IO uint16_t* adr = (__IO uint16_t*) (PAGE0_BASE_ADDRESS + 2 + (offset * 2));        // pointer to WORD = page address + WORD of PageStatus + Offset in bytes

    if(PageStatus0 == VALID_PAGE) {     // if page status says it's a valid page, read value and return it
        WORD val = *adr;
        return val;
    }
    
    // for other than VALID_PAGE states return default value    
    return defVal;
}    

WORD EE_WriteVariable(WORD offset, WORD newValue)
{
    FLASH_Status stat = FLASH_COMPLETE;

    volatile WORD *pCurrentValue =   (__IO uint16_t*) (PAGE0_BASE_ADDRESS + 2 + (offset * 2));
    WORD PageStatus0             = *((__IO uint16_t*)  PAGE0_BASE_ADDRESS);
    WORD currentValue            = *pCurrentValue;

    // first check is page is valid, and if the value did change
    if(PageStatus0 == VALID_PAGE && (currentValue == newValue)) {   // page is valid, value didn't change? Good, quit
        return FLASH_COMPLETE;
    }

    // if we got here, then either page is not valid, or the value changed
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR); 

    stat = FLASH_ErasePage(PAGE0_BASE_ADDRESS);                         // erase page
    if(stat != FLASH_COMPLETE) {
        return stat;
    }
    
    stat = FLASH_ProgramHalfWord(PAGE0_BASE_ADDRESS, VALID_PAGE);       // write VALID_PAGE marker
    if(stat != FLASH_COMPLETE) {
        return stat;
    }

    stat = FLASH_ProgramHalfWord((uint32_t) pCurrentValue, newValue);   // wite new vale
    if(stat != FLASH_COMPLETE) {
        return stat;
    }

    FLASH_Lock();
    return stat;
}

