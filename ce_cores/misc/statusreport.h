#ifndef _STATUSREPORT_H_
#define _STATUSREPORT_H_

#include <stdint.h>
#include "../chipinterface/chipinterfacedefs.h"

typedef struct {
    uint32_t    ipAddr;
    uint8_t     mac[6];
    char        fwVer[16];      // YYYY-MM-DD
    time_t      timestamp;      // store using time(NULL); then to human time:    struct tm *tm_info = localtime(&timestamp); char buffer[80]; strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    uint8_t     features;       // ikbd, fdd, acsi, scsi
    uint16_t    tosVersion;     // e.g. 0x0205
    int         scsiMachine;    // SCSI_MACHINE_TT | SCSI_MACHINE_FALCON

    std::string lastWrittenReport;
} TClientStatus;

extern TClientStatus statuses[MAX_CLIENTS];

class StatusReport {
public:
    static void createReportFiles(void);
    static void createSingleReportFile(int i);

    static int getIndexFromMac(uint8_t* mac, bool& isNew);
    static void storeIpAndFwVer(uint8_t* mac, uint32_t ipAddr, char* fwVer, uint8_t features);
    static void storeTosAndMachine(uint8_t* mac, uint16_t tosVersion, int scsiMachine);
};

#endif
