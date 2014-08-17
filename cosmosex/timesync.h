#ifndef _TIMESYNC_H_
#define _TIMESYNC_H_

#define INIT_NONE           0
#define INIT_NTP_FAILED     1
#define INIT_DATE_NOT_SET   2
#define INIT_OK             3

class TimeSync 
{
public:
    TimeSync();
    
    bool sync(void);

private:
    int         iInitState;
    long int    lTime;

    bool syncByNtp(void);
    void refreshNetworkDateNtp(void);
    
};

#endif
