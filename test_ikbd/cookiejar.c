#include "cookiejar.h"

unsigned char CookieJarWrite(unsigned long name, unsigned long value)
{
    register unsigned short retvalue asm("%d0"); 

    asm volatile
    (
        "move.l %1,D0\n\t"
        "move.l %2,D1\n\t"
        "jsr    CK_WriteJar\n\t"
    : "=r"(retvalue) /* outputs */
    : "g"(name),"g"(value) /* inputs */
    : "d1" /* clobbered regs */
    );
    return retvalue;
}

unsigned char CookieJarRead(unsigned long name, unsigned long* value)
{
    register unsigned short retvalue asm("%d0"); 

    asm volatile
    (
        "move.l %1,D0\n\t"
        "move.l %2,A0\n\t"
        "jsr    CK_ReadJar\n\t"
    : "=r"(retvalue) /* outputs */
    : "g"(name),"g"(value) /* inputs */
    : "a0" /* clobbered regs */
    );
    return retvalue;
}