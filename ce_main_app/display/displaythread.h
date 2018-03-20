#ifndef DISPLAYTHREAD_H
#define DISPLAYTHREAD_H

void display_init(void);
void display_deinit(void);
void display_print_center(const char *str);

#define DISP_LINE_HDD_IDS    0
#define DISP_LINE_HDD_TYPES  1
#define DISP_LINE_FLOPPY     2
#define DISP_LINE_IKDB       3
#define DISP_LINE_LAN        4
#define DISP_LINE_WLAN       5
#define DISP_LINE_COUNT      6       // how many disply lines we have? last index + 1

#define DISP_SCREEN_HDD1    {DISP_LINE_HDD_IDS,   DISP_LINE_FLOPPY, DISP_LINE_IKDB, DISP_LINE_LAN}
#define DISP_SCREEN_HDD2    {DISP_LINE_HDD_TYPES, DISP_LINE_FLOPPY, DISP_LINE_IKDB, DISP_LINE_WLAN}

void display_setLine(int displayLineId, const char *newLineString);

void *displayThreadCode(void *ptr);

#endif /* DISPLAYTHREAD_H */

