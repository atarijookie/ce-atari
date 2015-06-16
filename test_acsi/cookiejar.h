#ifndef COOKIEJAR_H
#define COOKIEJAR_H

extern unsigned char CookieJarWrite(unsigned long name, unsigned long value);
extern unsigned char CookieJarRead(unsigned long name, unsigned long* value);

#endif