#ifndef VERSION_H
#define VERSION_H

void showAppVersion(void);
int  getIntFromStr(const char *str, int len);

void showInt    (int value, int length);
void intToString(int value, int length, char *tmp);

void showIntWithPrepadding(int value, int fullLength, char prepadChar);

#endif