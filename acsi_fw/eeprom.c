
#include "stm32f10x_flash.h"
#include "eeprom.h"
#include "defs.h"

WORD EE_Init(void)
{
    uint16_t PageStatus0 = 6;
    uint16_t FlashStatus;

    // Get Page0 status
    PageStatus0 = (*(__IO uint16_t*) PAGE0_BASE_ADDRESS);

    // page erased or already used? good!
    if(PageStatus0 == VALID_PAGE || PageStatus0 == ERASED) {    // valid page or erased page? good
        return FLASH_COMPLETE;
    }
    
    // page in some other state? format it
    FlashStatus = FLASH_ErasePage(PAGE0_BASE_ADDRESS);
    return FlashStatus;
}

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
    WORD curValue;
    FLASH_Status FlashStatus = FLASH_COMPLETE;
    
    WORD PageStatus0 = (*(__IO uint16_t*) PAGE0_BASE_ADDRESS);
    __IO uint16_t* adr = (__IO uint16_t*) (PAGE0_BASE_ADDRESS + 2 + (offset * 2));        // pointer to WORD = page address + WORD of PageStatus + Offset in bytes

    if(PageStatus0 != VALID_PAGE) {             // not valid page? make it valid
        FlashStatus = FLASH_ProgramHalfWord(PAGE0_BASE_ADDRESS, VALID_PAGE);
        
        if(FlashStatus != FLASH_COMPLETE) {     // write failed? fail
            return FlashStatus;
        }
    }
    
    curValue = *adr;                            // read current value
    
    if(curValue != newValue) {                  // current value isn't the same as new one? 
        FlashStatus = FLASH_ProgramHalfWord((uint32_t) adr, newValue);
    }

    return FlashStatus;
}

