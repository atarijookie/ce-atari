#ifndef _OUT_H_
#define _OUT_H_

void out_tr_b (WORD testNo, char *testName, BYTE result);
void out_tr_bw(WORD testNo, char *testName, BYTE result, WORD errorCode);

void out_s   (char *str1);
void out_ss  (char *str1, char *str2);
void out_sc  (char *str1, char c);
void out_sw  (char *str1, WORD w1);
void out_swsw(char *str1, WORD w1, char *str2, WORD w2);

void initBuffer(void);
void appendToBuf(char *str);
void writeBufferToFile(void);
void deinitBuffer(void);

#endif
