#ifndef DISPLAYTHREAD_H
#define DISPLAYTHREAD_H

//----------------------------
// display related stuff
void display_init(void);
void display_deinit(void);
void display_print_center(char *str);

#define DISP_LINE_EMPTY      0
#define DISP_LINE_HDD_IDS    1
#define DISP_LINE_HDD_TYPES  2
#define DISP_LINE_FLOPPY     3
#define DISP_LINE_IKDB       4
#define DISP_LINE_LAN        5
#define DISP_LINE_WLAN       6
#define DISP_LINE_TRAN_SETT  7
#define DISP_LINE_CONF_SHAR  8
#define DISP_LINE_TRAN_DRIV  9
#define DISP_LINE_DATETIME   10
#define DISP_LINE_RECOVERY1  11
#define DISP_LINE_RECOVERY2  12
#define DISP_LINE_COUNT      13     // how many disply lines we have? last index + 1

#define DISP_SCREEN_HDD1_IDX    0
#define DISP_SCREEN_HDD2_IDX    1
#define DISP_SCREEN_TRANS_IDX   2
#define DISP_SCREEN_RECOVERY    3
#define DISP_SCREEN_COUNT       4   // how many different screens we have?

#define DISP_SCREEN_HDD1_LINES     {DISP_LINE_HDD_IDS,   DISP_LINE_FLOPPY, DISP_LINE_IKDB, DISP_LINE_LAN}
#define DISP_SCREEN_HDD2_LINES     {DISP_LINE_HDD_TYPES, DISP_LINE_FLOPPY, DISP_LINE_IKDB, DISP_LINE_WLAN}
#define DISP_SCREEN_TRANS_LINES    {DISP_LINE_TRAN_SETT, DISP_LINE_CONF_SHAR, DISP_LINE_TRAN_DRIV, DISP_LINE_DATETIME}
#define DISP_SCREEN_RECOVERY_LINES {DISP_LINE_EMPTY,     DISP_LINE_RECOVERY1, DISP_LINE_RECOVERY2, DISP_LINE_EMPTY}

void display_setLine(int displayLineId, const char *newLineString);
void display_showNow(int screenIndex);

void *displayThreadCode(void *ptr);

//----------------------------
// beeper related stuff
#define BEEP_SHORT              0
#define BEEP_MEDIUM             1
#define BEEP_LONG               2

#define BEEP_RELOAD_SETTINGS    10

#define BEEP_FLOPPY_SEEK        0x80

void beeper_beep(int beepLen);
void beeper_floppySeek(int trackCount);
void beeper_reloadSettings(void);

#endif /* DISPLAYTHREAD_H */

