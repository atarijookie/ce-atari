// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef _TIMESYNC_H_
#define _TIMESYNC_H_

void *timesyncThreadCode(void *ptr);

class TimeSync 
{
public:
    TimeSync();
    ~TimeSync();
    
    bool waitingForDataOnFd(void);
    int getFd(void) { return s; };
    void process(bool dataToRead);
    DWORD nextProcessTime;

private:
    void sendNtpPacket(void);
    void readNtpPacket(void);
    void sendWebRequest(void);
    void checkWebResponse(void);

    /*
    INIT_NTP -> send ntp packet -> NTP_WAIT_PACKET{n} -> receive -> DATE_SET
                                                      -> NTP_FAILED
    INIT_WEB -> send http request -> WEB_WAIT_RESPONSE -> receive -> DATE_SET
                                                       -> WEB_FAILED

    */
    enum TimeSyncStateEnum {
        INIT=0, DATE_SET, DATE_FAILED,
        INIT_NTP, NTP_WAIT_PACKET1, NTP_WAIT_PACKET2, NTP_WAIT_PACKET3, NTP_FAILED,
        INIT_WEB, WEB_WAIT_RESPONSE, WEB_FAILED
      } eState;
    int s;    /* socket */
};

#endif
