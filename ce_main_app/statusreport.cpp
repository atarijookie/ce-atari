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
#include "ikbd.h"
#include "floppy/imagesilo.h"

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
    // general section
    startSection(report, "general status", reportFormat);

    dumpPair(report, "HDD interface type",      (hwConfig.hddIface == HDD_IF_ACSI) ? "ACSI" : "SCSI", reportFormat);
    dumpPair(report, "eth0  up and running",    state_eth0  ? "yes" : "no", reportFormat);
    dumpPair(report, "wlan0 up and running",    state_wlan0 ? "yes" : "no", reportFormat);

    char humanTime[128];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(humanTime, "%04d-%02d-%02d, %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    dumpPair(report, "Current date & time",     humanTime, reportFormat);

    endSection  (report, reportFormat);

    //------------------
    // ikdb emulation
    startSection(report, "IKBD emulation", reportFormat);

    if(ikbdDevs[INTYPE_MOUSE].fd        > 0) dumpPair(report, "USB mouse",          ikbdDevs[INTYPE_MOUSE].devPath,         reportFormat);
    if(ikbdDevs[INTYPE_KEYBOARD].fd     > 0) dumpPair(report, "USB keyboard",       ikbdDevs[INTYPE_KEYBOARD].devPath,      reportFormat);
    if(ikbdDevs[INTYPE_JOYSTICK1].fd    > 0) dumpPair(report, "USB joystick 1",     ikbdDevs[INTYPE_JOYSTICK1].devPath,     reportFormat);
    if(ikbdDevs[INTYPE_JOYSTICK2].fd    > 0) dumpPair(report, "USB joystick 2",     ikbdDevs[INTYPE_JOYSTICK2].devPath,     reportFormat);
    if(ikbdDevs[INTYPE_VDEVMOUSE].fd    > 0) dumpPair(report, "virtual mouse",      ikbdDevs[INTYPE_VDEVMOUSE].devPath,     reportFormat);
    if(ikbdDevs[INTYPE_VDEVKEYBOARD].fd > 0) dumpPair(report, "virtual keyboard",   ikbdDevs[INTYPE_VDEVKEYBOARD].devPath,  reportFormat);

    if(noOfElements < 1) dumpPair(report, "no input devices", "0", reportFormat);

    endSection  (report, reportFormat);

    //------------------
    // USB drives
    startSection(report, "USB drives", reportFormat);

    pthread_mutex_lock(&shared.mtxTranslated);
   
    dumpPair(report, "Mounting USB drives as", (shared.mountRawNotTrans) ? "RAW" : "translated", reportFormat, TEXT_COL1_WIDTH, 60);

    for(int i=2; i<MAX_DRIVES; i++) {
        if(shared.translated->driveIsEnabled(i)) {
            std::string driveName = std::string("Drive X");
            driveName[6] = 'A' + i;

            std::string reportString;
            shared.translated->driveGetReport(i, reportString);

            dumpPair(report, driveName.c_str(), reportString.c_str(), reportFormat, TEXT_COL1_WIDTH, 60);
        }
    }
    
    pthread_mutex_unlock(&shared.mtxTranslated);

    endSection  (report, reportFormat);
    
    //------------------
    // floppy images
    startSection(report, "Floppy image slots", reportFormat);

    char tmp[32];
    if(floppyImageSelected >= 0) {
        sprintf(tmp, "%d", floppyImageSelected + 1);
    } else {
        strcpy(tmp, "none");
    }

    dumpPair(report, "Selected floppy image", tmp, reportFormat);

    for(int i=0; i<3; i++) {
        bool selected = (i == floppyImageSelected);

        sprintf(tmp, "Slot %d %s", i + 1, selected ? "<- selected" : "");

        const char *imageFile = (floppyImages[i].imageFile.length() > 0) ? floppyImages[i].imageFile.c_str() : "(empty)";
        dumpPair(report, tmp, imageFile, reportFormat);
    }

    endSection  (report, reportFormat);

    //------------------
    // chips and interfaces
    startSection   (report, "chips and interfaces live status",        reportFormat);
    putStatusHeader(report, reportFormat);
    dumpStatus     (report, "Hans  chip",          statuses.hans,      reportFormat);
    dumpStatus     (report, "Franz chip",          statuses.franz,     reportFormat);
    dumpStatus     (report, "Hard Drive IF",       statuses.hdd,       reportFormat);
    dumpStatus     (report, "Floppy IF",           statuses.fdd,       reportFormat);
    dumpStatus     (report, "IKBD from ST",        statuses.ikbdSt,    reportFormat);
    dumpStatus     (report, "IKBD from USB",       statuses.ikbdUsb,   reportFormat);
    endSection     (report,                                            reportFormat);

    //------------------

    endReport   (report, reportFormat);
}

void StatusReport::putStatusHeader(std::string &report, int reportFormat)
{
    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT:
        report += std::string( fixStringToLength("What chip or interface",  TEXT_COL1_WIDTH)) + std::string(": ");
        report += std::string( fixStringToLength("When was alive sign",     TEXT_COL2_WIDTH)) + std::string(" - ");
        report += std::string( fixStringToLength("What alive sign",         TEXT_COL3_WIDTH)) + std::string("\n");
        break;

        case REPORTFORMAT_HTML:
        report += "<tr bgcolor='#cccccc'>";
        report += "    <td>What chip or interface </td>";
        report += "    <td>When was alive sign    </td>";
        report += "    <td>Is that good or bad?   </td>";
        report += "    <td>What was the alive sign</td> </tr>\n";
        break;
    }
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
    std::string aliveStr;

    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT:
        report += std::string( fixStringToLength(desciprion, TEXT_COL1_WIDTH) ) + std::string(": ");

        aliveStr = aliveAgoString + std::string(" (") + (good ? "good" : "bad") + std::string(")");
        report += fixStringToLength(aliveStr.c_str(),   TEXT_COL2_WIDTH)        + std::string(" - ");

        report += fixStringToLength(aliveSignString,    TEXT_COL3_WIDTH) + std::string("\n");
        break;

        case REPORTFORMAT_HTML:
        report += "<tr><td bgcolor='#cccccc'>";
        report += desciprion;
        report += "</td><td><center>";
        report += aliveAgoString;
        report += "</center></td><td><center>";
        report += good ? "<font color='#00aa00'>good" : "<font color='#cc0000'>bad";
        report += "</font></center></td><td><center>";
        report += aliveSignString;
        report += "</center></td></tr>";
        break;

        case REPORTFORMAT_JSON:
        if(noOfElements > 0) {
            report += ",";
        }

        report += "{\"desc\":\"";
        report += desciprion;
        report += "\", \"liveAgo\":\"";
        report += aliveAgoString;
        report += "\", \"good\":\"";
        report += good ? "true" : "false";
        report += "\", \"aliveSign\":\"";
        report += aliveSignString;
        report += "\"}";
        break;
    }

    noOfElements++;
}

void StatusReport::dumpPair(std::string &report, const char *key, const char *value, int reportFormat, int len1, int len2)
{
    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT: 
        report += fixStringToLength(key,    len1);
        report += ": ";
        report += fixStringToLength(value,  len2);
        report += "\n";
        break;

        case REPORTFORMAT_HTML:
        report += "<tr><td bgcolor='#cccccc'>";
        report += key;
        report += "</td><td><center>";
        report += value;
        report += "</center></td></tr>";
        break;

        case REPORTFORMAT_JSON:
        if(noOfElements > 0) {
            report += ",";
        }

        report += "{\"key\":\"";
        report += key;
        report += "\", \"value\":\"";
        report += value;
        report += "\"}";
        break;
    }

    noOfElements++;
}

void StatusReport::startSection(std::string &report, const char *sectionName, int reportFormat)
{
    noOfElements = 0;

    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT: 
        report += sectionName;
        report += "\n----------------------------------------\n";
        break;

        case REPORTFORMAT_HTML:
        report += "<b>";
        report += sectionName;
        report += "</b><br> <table border=1 cellspacing=0 cellpadding=5>";
        break;

        case REPORTFORMAT_JSON:
        if(noOfSections > 0) {
            report += ",";
        }

        report += "\"";
        report += sectionName;
        report += "\":[";
        break;
    }

    noOfSections++;
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
    noOfElements = 0;
    noOfSections = 0;

    switch(reportFormat) {
    case REPORTFORMAT_RAW_TEXT: 
        report += "CosmosEx device report\n";
        report += "----------------------\n\n";
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

char *StatusReport::fixStringToLength(const char *inStr, int outLen)
{
    static char tmp[100];
    
    memset(tmp, ' ', outLen);                               // first fill it with spaces
    tmp[outLen] = 0;

    int inLen = strlen(inStr);
    int cpLen = (inLen < outLen) ? inLen : outLen;
    strncpy(tmp, inStr, cpLen);

    return tmp;
}



