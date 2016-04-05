#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "global.h"
#include "debug.h"
#include "update.h"
#include "periodicthread.h"

#include "statusreport.h"

extern THwConfig    hwConfig;
extern TFlags       flags;

extern DebugVars    dbgVars;

extern SharedObjects shared;
extern volatile bool floppyEncodingRunning;

volatile TStatuses statuses;

extern bool state_eth0;
extern bool state_wlan0;

void StatusReport::createReport(std::string &report, int reportFormat)
{
    report = "";
    startReport (report, reportFormat);

    //------------------
    startSection(report, "general status", reportFormat);

    dumpPair(report, "HDD interface type",      (hwConfig.hddIface == HDD_IF_ACSI) ? "ACSI" : "SCSI", reportFormat);
    dumpPair(report, "eth0 up and running",     state_eth0  ? "yes" : "no", reportFormat);
    dumpPair(report, "wlan0 up and running",    state_wlan0 ? "yes" : "no", reportFormat);

    endSection  (report, reportFormat);

    //------------------
    startSection(report, "chips and interfaces live status",        reportFormat);
    dumpStatus  (report, "Hans  chip",          statuses.hans,      reportFormat);
    dumpStatus  (report, "Franz chip",          statuses.franz,     reportFormat);
    dumpStatus  (report, "Hard Drive IF",       statuses.hdd,       reportFormat);
    dumpStatus  (report, "Floppy IF",           statuses.fdd,       reportFormat);
    dumpStatus  (report, "IKBD from ST",        statuses.ikbdSt,    reportFormat);
    dumpStatus  (report, "IKBD from USB",       statuses.ikbdUsb,   reportFormat);
    endSection  (report,                                            reportFormat);

    //------------------
    endReport   (report, reportFormat);
}

void StatusReport::dumpStatus(std::string &report, const char *desciprion, volatile TStatus &status, int reportFormat)
{
    DWORD now       = Utils::getCurrentMs();
    int   aliveAgo;
    char  aliveAgoString[64];
    bool  good;

    if(status.aliveTime == 0) {     // never received alive sign?
        aliveAgo    = -1;           // never
        good        = false;
        strcpy(aliveAgoString, "never");
    } else {
        DWORD diffMs    = now - status.aliveTime;       // calculate how many ms have passed since last alive sign
        aliveAgo        = diffMs / 1000;                // convert ms to s
        good            = true;

        if(aliveAgo < 1) {
            strcpy(aliveAgoString, "few ms ago");
        } else if(aliveAgo < 60) {
            sprintf(aliveAgoString, "%d seconds ago", aliveAgo);
        } else {
            int aliveMinutes = aliveAgo / 60;
            sprintf(aliveAgoString, "%d minute%s ago", aliveMinutes, (aliveMinutes > 1) ? "s" : "");
        }
    }

    const char *aliveSignString = aliveSignIntToString(status.aliveSign);

    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT: 
        report += desciprion;
        report += ": ";
        report += aliveAgoString;
        report += " (";
        report += good ? "good" : "bad";
        report += ") - ";
        report += aliveSignString;
        report += "\n";
        break;

        case REPORTFORMAT_HTML:
        report += "<tr><td>";
        report += desciprion;
        report += "</td><td>";
        report += aliveAgoString;
        report += "</td><td>";
        report += good ? "good" : "bad";
        report += "</td><td>";
        report += aliveSignString;
        report += "</td></tr>";
        break;

        case REPORTFORMAT_JSON:
        report += "{\"desc\":\"";
        report += desciprion;
        report += "\", \"liveAgo\":\"";
        report += aliveAgoString;
        report += "\", \"good\":\"";
        report += good ? "true" : "false";
        report += "\", \"aliveSign\":\"";
        report += aliveSignString;
        report += "\"}\n";
        break;
    }
}

void StatusReport::dumpPair(std::string &report, const char *key, const char *value, int reportFormat)
{
    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT: 
        report += key;
        report += ": ";
        report += value;
        report += "\n";
        break;

        case REPORTFORMAT_HTML:
        report += "<tr><td>";
        report += key;
        report += "</td><td>";
        report += value;
        report += "</td></tr>";
        break;

        case REPORTFORMAT_JSON:
        report += "{\"key\":\"";
        report += key;
        report += "\", \"value\":\"";
        report += value;
        report += "\"}\n";
        break;
    }
}

void StatusReport::startSection(std::string &report, const char *sectionName, int reportFormat)
{
    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT: 
        report += sectionName;  
        report += "\n";
        break;

        case REPORTFORMAT_HTML:
        report += "<b>";
        report += sectionName;
        report += "</b><br> <table border=1 cellspacing=0 cellpadding=5>";
        break;

        case REPORTFORMAT_JSON:
        report += "\"";
        report += sectionName;
        report += "\":[";
        break;
    }
}

void StatusReport::endSection(std::string &report, int reportFormat)
{
    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT: 
        report += "\n\n";
        break;

        case REPORTFORMAT_HTML:
        report += "</table><br><br>\n";
        break;

        case REPORTFORMAT_JSON:
        report += "]\n";
        break;
    }
}

void StatusReport::startReport(std::string &report, int reportFormat)
{
    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT: 
        report += "CosmosEx device report\n";
        report += "----------------------\n";
        break;

        case REPORTFORMAT_HTML:
        report += "<html><head><title>CosmosEx device report</title></head>";
        report += "<body><b>CosmosEx device report</b><br><br>";
        break;

        case REPORTFORMAT_JSON:
        report += "{";
        break;
    }
}

void StatusReport::endReport(std::string &report, int reportFormat)
{
    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT: 
        report += "\n";
        break;

        case REPORTFORMAT_HTML:
        report += "\n</body></html>";
        break;

        case REPORTFORMAT_JSON:
        report += "}\n";
        break;
    }
}

const char *StatusReport::aliveSignIntToString(int aliveSign)
{
    switch(aliveSign) {
        case ALIVE_FWINFO:      return "FW info";
        case ALIVE_CMD:         return "command";
        case ALIVE_RW:          return "read or write cmd";
        case ALIVE_READ:        return "read command";
        case ALIVE_WRITE:       return "write command";
        case ALIVE_IKBD_CMD:    return "IKBC command";
        case ALIVE_KEYDOWN:     return "key pressed";
        case ALIVE_MOUSEMOVE:   return "mouse moved";
        case ALIVE_MOUSEBTN:    return "mouse button pressed";
        case ALIVE_JOYMOVE:     return "joy moved";
        case ALIVE_JOYBTN:      return "joy button pressed";

        case ALIVE_DEAD:    
        default:    
            return "nothing";
    }
}


