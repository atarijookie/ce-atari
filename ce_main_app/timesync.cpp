// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <iostream>

#include "utils.h"
#include "debug.h"
#include "timesync.h"
#include "settings.h"
#include "downloader.h"

#define NTP_PORT 123
#define TIMESYNC_URL_WEB    "http://joo.kie.sk/cosmosex/time.php"
volatile uint8_t timeSyncStatusByte;
volatile bool do_timeSync = false; //TODO !

TimeSync::TimeSync() : eState(INIT), s(-1)
{
    nextProcessTime = Utils::getEndTime(1000); // start to work in 1 sec
}

TimeSync::~TimeSync()
{
    if(s >= 0) {
        close(s);
    }
}

bool TimeSync::waitingForDataOnFd(void)
{
    switch(eState) {
    case NTP_WAIT_PACKET1:
    case NTP_WAIT_PACKET2:
    case NTP_WAIT_PACKET3:
        return true;
    default:
        return false;
    }
}

void TimeSync::process(bool dataToRead)
{
    Debug::out(LOG_DEBUG, "TimeSync::process(%s) entering with eState=%d nextProcessTime=%u", dataToRead ? "true" : "false", (int)eState, nextProcessTime);
    switch(eState) {
    case INIT:
    case INIT_NTP:
        sendNtpPacket();
        break;
    case NTP_WAIT_PACKET1:
    case NTP_WAIT_PACKET2:
        if(dataToRead) {
            readNtpPacket();
        } else {
            sendNtpPacket();
        }
        break;
    case NTP_WAIT_PACKET3:
        if(dataToRead) {
            readNtpPacket();
        } else {
            eState = NTP_FAILED;
        }
        break;
    case NTP_FAILED:
    case INIT_WEB:
        sendWebRequest();
        break;
    case WEB_WAIT_RESPONSE:
        checkWebResponse();
        break;
    case WEB_FAILED:
    case DATE_FAILED:
        if(s >= 0) {
            close(s);
            s = -1;
        }
        nextProcessTime = Utils::getEndTime(60*1000);// retry in  1 minute
        eState = INIT;
        break;
    case DATE_SET:
        if(s >= 0) {
            close(s);
            s = -1;
        }
        nextProcessTime = Utils::getEndTime(30*60*1000);// do it again in 30 minutes
        eState = INIT;
        break;
    }
    Debug::out(LOG_DEBUG, "TimeSync::process(%s) exiting with eState=%d nextProcessTime=%u", dataToRead ? "true" : "false", (int)eState, nextProcessTime);
}

//from http://stackoverflow.com/questions/9326677/is-there-any-c-c-library-to-connect-with-a-remote-ntp-server/19835285#19835285
void TimeSync::sendNtpPacket(void)
{
    unsigned char msg[48]={010,0,0,0,0,0,0,0,0};    // the packet we send
    struct sockaddr_in server_addr;
    ssize_t n;

    if(s < 0) {
        s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(s < 0) {
            Debug::out(LOG_ERROR, "TimeSync::sendNtpPacket(): socket : %s", strerror(errno));
            eState = NTP_FAILED;
        }
    }
    Settings settings;
    std::string ntpServer = settings.getString("TIME_NTP_SERVER", "200.20.186.76");

    //#convert hostname to ipaddress if needed
    memset( &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ntpServer.c_str());
    server_addr.sin_port = htons(NTP_PORT);

    /*
     * build a message.  Our message is all zeros except for a one in the
     * protocol version field
     * msg[] in binary is 00 001 000 00000000
     * it should be a total of 48 bytes long
     */
    // send the data
    Debug::out(LOG_DEBUG, "TimeSync: requesting date from NTP %s:%d", ntpServer.c_str(),NTP_PORT);

    n = sendto(s,msg,sizeof(msg),0,(struct sockaddr *)&server_addr,sizeof(server_addr));
    if(n < 0) {
        Debug::out(LOG_DEBUG, "TimeSync: could not set UDP packet: %s",strerror(errno));
        eState = NTP_FAILED;
        return;
    }

    switch(eState) {
    case NTP_WAIT_PACKET1:
        eState = NTP_WAIT_PACKET2;
        break;
    case NTP_WAIT_PACKET2:
        eState = NTP_WAIT_PACKET3;
        break;
    default:
        eState = NTP_WAIT_PACKET1;
    }
    eState = static_cast<TimeSyncStateEnum>(static_cast<int>(eState) + 1);
    nextProcessTime = Utils::getEndTime(5000);
}

void TimeSync::readNtpPacket(void)
{
    ssize_t n;
    long int tmit;   // the time -- This is a time_t sort of
    time_t t;
    struct sockaddr_storage saddr;
    socklen_t saddr_len = sizeof(saddr);
    uint32_t buf[1500/sizeof(uint32_t)];

    n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&saddr, &saddr_len);   // try to read data (should succeed)
    if(n < 0) {                                               // failed? quit
        Debug::out(LOG_ERROR, "TimeSync: could not recieve packet: %s", strerror(errno));
        eState = NTP_FAILED;
        return;
    }
    char addr_str[48] = "";
    unsigned short port = 0;
    if(saddr.ss_family == AF_INET) {
        port = ntohs(((struct sockaddr_in *)&saddr)->sin_port);
        inet_ntop(saddr.ss_family, &((struct sockaddr_in *)&saddr)->sin_addr, addr_str, sizeof(addr_str));
    }
    Debug::out(LOG_DEBUG, "TimeSync: NTP Packet received from %s:%hu %ldbytes", addr_str, port, (long)n);

    /*
     * The high word of transmit time is the 10th word we get back
     * tmit is the time in seconds not accounting for network delays which
     * should be way less than a second if this is a local NTP server
     */
    tmit = ntohl((time_t)buf[4]);    //# get transmit time

    /*
     * Convert time to unix standard time NTP is number of seconds since 0000
     * UT on 1 January 1900 unix time is seconds since 0000 UT on 1 January
     * 1970 There has been a trend to add a 2 leap seconds every 3 years.
     * Leap seconds are only an issue the last second of the month in June and
     * December if you don't try to set the clock then it can be ignored but
     * this is importaint to people who coordinate times with GPS clock sources.
     */

    tmit -= 2208988800U;
    //printf("tmit=%d\n",tmit);
    /* use unix library function to show me the local time (it takes care
     * of timezone issues for both north and south of the equator and places
     * that do Summer time/ Daylight savings time.
     */

    //#compare to system time
    Debug::out(LOG_DEBUG, "TimeSync: NTP time is %s", ctime(&tmit));
    t = time(NULL);
    Debug::out(LOG_DEBUG, "TimeSync: System time is %ld seconds off", (t-tmit));

    //------------
    // check date validity, reject if it seems bad
    struct tm gmTime;
    memset(&gmTime, 0, sizeof(gmTime));         // clear struct
    gmtime_r(&tmit, &gmTime);                  // convert int time to struct time
    int year = gmTime.tm_year + 1900;
    Debug::out(LOG_DEBUG, "TimeSync: date from NTP is: %04d-%02d-%02d", year, gmTime.tm_mon + 1, gmTime.tm_mday);

    if(year < 2018 || year > 2050) {
        Debug::out(LOG_ERROR, "TimeSync: NTP year %04d seems to be invalid (it's not between 2018 and 2050), syncByNtp fail", year);
        eState = NTP_FAILED;
        return;
    }

    timeval tv;
    tv.tv_sec   = tmit;
    tv.tv_usec  = 0;

    if(settimeofday(&tv,NULL) < 0) {
        Debug::out(LOG_ERROR, "TimeSync: settimeofday(): %s", strerror(errno));
        eState = NTP_FAILED;
        return;
    }

    Debug::out(LOG_DEBUG, "TimeSync: date set to %d", tmit);
    eState = DATE_SET;
}

void TimeSync::sendWebRequest(void)
{
    timeSyncStatusByte = DWNSTATUS_WAITING;

    TDownloadRequest tdr;
    tdr.srcUrl          = TIMESYNC_URL_WEB;
    tdr.dstDir          = "/tmp/";
    tdr.downloadType    = DWNTYPE_TIMESYNC;
    tdr.checksum        = 0;                        // special case - don't check checsum
    tdr.pStatusByte     = &timeSyncStatusByte;      // update status byte
    Downloader::add(tdr);
    Debug::out(LOG_DEBUG, "TimeSync::sendWebRequest() %s", TIMESYNC_URL_WEB);
    eState = WEB_WAIT_RESPONSE;
    nextProcessTime = Utils::getEndTime(300);
}

void TimeSync::checkWebResponse()
{
    if(timeSyncStatusByte == DWNSTATUS_DOWNLOAD_FAIL) {
        Debug::out(LOG_DEBUG, "TimeSync: couldn't get the time from web server...");
        eState = WEB_FAILED;
        return ;
    }

    if(timeSyncStatusByte == DWNSTATUS_DOWNLOAD_OK) {
        // if we got here, the file got downloaded to tmp
        Debug::out(LOG_DEBUG, "TimeSync: set time from downloaded /tmp/file.php");
        system("date -s ` tail -n 1 /tmp/time.php | cut -d ' ' -f 3 ` > /tmp/timesync.log 2>&1");
        eState = DATE_SET;
    } else {
        // WAIT 300 ms
        eState = WEB_WAIT_RESPONSE;
        nextProcessTime = Utils::getEndTime(300);
    }
}
