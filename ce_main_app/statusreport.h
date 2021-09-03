#ifndef _STATUSREPORT_H_
#define _STATUSREPORT_H_

#include <stdint.h>

// when not received anything
#define ALIVE_DEAD      0

// for Franz & Hans - if the chips are communicating
#define ALIVE_FWINFO    0x10
#define ALIVE_CMD       0x11

// ACSI / SCSI & FDD interface - if ST is communicating through them
#define ALIVE_RW        0x20
#define ALIVE_READ      0x21
#define ALIVE_WRITE     0x22

// IKBD interface
#define ALIVE_IKBD_CMD  0x30
#define ALIVE_KEYDOWN   0x31
#define ALIVE_MOUSEVENT 0x32
#define ALIVE_JOYEVENT  0x33

/*
Should also report:
- ACSI or SCSI interface selected
- component versions (main app, hans, franz, xilinx)
- current date / time on CE
- has ethernet and wifi connection (if up, IP?)
- got mouse, keyboard, joy?
- mounted usb drives & letters
- RAW / translated mount of USB drives
- mounted FDD images, selected FDD slot
*/

typedef struct {
    uint32_t   aliveTime;
    uint8_t    aliveSign;
} TStatus;

typedef struct {
    TStatus hans;
    TStatus franz;

    TStatus hdd;
    TStatus fdd;

    TStatus ikbdSt;
    TStatus ikbdUsb;
} TStatuses;

extern volatile TStatuses statuses;

#define REPORTFORMAT_RAW_TEXT       0
#define REPORTFORMAT_HTML_FULL      1
#define REPORTFORMAT_HTML_ONLYBODY  2
#define REPORTFORMAT_JSON           3

#define TEXT_COL1_WIDTH     40
#define TEXT_COL2_WIDTH     20
#define TEXT_COL3_WIDTH     20

class StatusReport {
public:
    void createReport(std::string &report, int reportFormat);

private:
    int  noOfElements;
    int  noOfSections;

    void startReport    (std::string &report, int reportFormat);
    void endReport      (std::string &report, int reportFormat);

    void startSection   (std::string &report, const char *sectionName,  int reportFormat);
    void endSection     (std::string &report,                           int reportFormat);

    void putStatusHeader(std::string &report, int reportFormat);
    void dumpStatus     (std::string &report, const char *desciprion, volatile TStatus &status, int reportFormat);
    void dumpPair       (std::string &report, const char *key,               const char *value, int reportFormat, bool centerValue=true, int len1=TEXT_COL1_WIDTH, int len2=TEXT_COL2_WIDTH);

    const char *aliveSignIntToString(int aliveSign);
          char *fixStringToLength(const char *inStr, int len);
};

#endif

