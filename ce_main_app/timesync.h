#ifndef _TIMESYNC_H_
#define _TIMESYNC_H_

void *timesyncThreadCode(void *ptr);

class TimeSync 
{
public:
    TimeSync();
    
    bool sync(void);

private:
    enum {INIT_NONE=0, INIT_NTP_FAILED=1, INIT_DATE_NOT_SET=2, INIT_OK=3} eInitState;
    long int    lTime;

    bool syncByNtp(void);
    void refreshNetworkDateNtp(void);

    bool syncByWeb(void);
};

#endif
