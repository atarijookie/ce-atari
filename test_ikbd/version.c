/*--------------------------------------------------*/
#include <mint/osbind.h> 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "version.h"
/*--------------------------------------------------*/

void showAppVersion(void)
{
    char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char const *buildDate = __DATE__;
    
    int year = 0, month = 0, day = 0;
    int i;
    for(i=0; i<12; i++) {
        if(strncmp(months[i], buildDate, 3) == 0) {
            month = i + 1;
            break;
        }
    }
    
    day     = getIntFromStr(buildDate + 4, 2);
    year    = getIntFromStr(buildDate + 7, 4);
    
    if(day > 0 && month > 0 && year > 0) {
        showInt(year, 4);
        (void) Cconout('-');
        showInt(month, 2);
        (void) Cconout('-');
        showInt(day, 2);
    } else {
        (void) Cconws("YYYY-MM-DD");
    }
}

/*--------------------------------------------------*/

int getIntFromStr(const char *str, int len)
{
    int i;
    int val = 0;
    
    for(i=0; i<len; i++) {
        int digit;
        
        if(str[i] >= '0' && str[i] <= '9') {
            digit = str[i] - '0';
        } else {
            digit = 0;
        }
    
        val *= 10;
        val += digit;
    }
    
    return val;
}

/*--------------------------------------------------*/

void showIntWithPrepadding(int value, int fullLength, char prepadChar)
{
    int i, padCount;
    int digitsLength = countIntDigits(value);
    
    padCount = fullLength - digitsLength;       // count how many chars we need for pre-padding
    
    for(i=0; i<padCount; i++) {                 // prepad
        Cconout(prepadChar);
    }
    
    showInt(value, digitsLength);               // display the rest
}
