#ifndef DISPLAYTHREAD_H
#define DISPLAYTHREAD_H

//----------------------------
// display related stuff
void display_init(void);
void display_deinit(void);
void display_print_center(const char *str);

#define DISP_LINE_HDD_IDS    0
#define DISP_LINE_HDD_TYPES  1
#define DISP_LINE_FLOPPY     2
#define DISP_LINE_IKDB       3
#define DISP_LINE_LAN        4
#define DISP_LINE_WLAN       5
#define DISP_LINE_TRAN_SETT  6
#define DISP_LINE_CONF_SHAR  7
#define DISP_LINE_TRAN_DRIV  8
#define DISP_LINE_DATETIME   9
#define DISP_LINE_COUNT      10       // how many disply lines we have? last index + 1

#define DISP_SCREEN_HDD1_IDX    0
#define DISP_SCREEN_HDD2_IDX    1
#define DISP_SCREEN_TRANS_IDX   2
#define DISP_SCREEN_COUNT       3   // how many different screens we have?

#define DISP_SCREEN_HDD1_LINES  {DISP_LINE_HDD_IDS,   DISP_LINE_FLOPPY, DISP_LINE_IKDB, DISP_LINE_LAN}
#define DISP_SCREEN_HDD2_LINES  {DISP_LINE_HDD_TYPES, DISP_LINE_FLOPPY, DISP_LINE_IKDB, DISP_LINE_WLAN}
#define DISP_SCREEN_TRANS_LINES {DISP_LINE_TRAN_SETT, DISP_LINE_CONF_SHAR, DISP_LINE_TRAN_DRIV, DISP_LINE_DATETIME}

void display_setLine(int displayLineId, const char *newLineString);
void display_showNow(int screenIndex);

void *displayThreadCode(void *ptr);

//----------------------------
// beeper related stuff
#define BEEP_SHORT       0
#define BEEP_MEDIUM      1
#define BEEP_LONG        2

#define BEEP_FLOPPY_SEEK 0x80

void beeper_beep(int beepLen);
void beeper_floppySeek(int trackCount);

#endif /* DISPLAYTHREAD_H */

