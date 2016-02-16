#ifndef _OUT_H_
#define _OUT_H_

void byteToHex ( BYTE val, char *bfr);
void wordToHex ( WORD val, char *bfr);
void dwordToHex(DWORD val, char *bfr);

void out_tr_b (WORD testNo, char *testName, BYTE result);
void out_tr_bw(WORD testNo, char *testName, BYTE result, WORD errorCode);
void out_tr_bd(WORD testNo, char *testName, BYTE result, DWORD errorCode);

void out_tr_eb (WORD testNo, char *testName, char *errorString, BYTE result);
void out_tr_ebw(WORD testNo, char *testName, char *errorString, BYTE result, WORD errorCode);

void out_test_header         (WORD testNo, char *testName);
void out_result              (BYTE result);
void out_result_error        (BYTE result, WORD errorCode);
void out_result_error_string (BYTE result, WORD errorCode, char *errorStr);
void out_result_string       (BYTE result, char *errorStr);

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
