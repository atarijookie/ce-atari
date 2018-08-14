#include "dateacsicommand.h"
#include "global.h"
#include "debug.h"

DateAcsiCommand::DateAcsiCommand(DataTrans *dt, ConfigService *ds):dataTrans(dt),pxDateService(ds)
{
}

DateAcsiCommand::~DateAcsiCommand()
{
}

void DateAcsiCommand::processCommand(BYTE *command)
{
    cmd = command;

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "DateAcsiCommand::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    switch(cmd[4]) {
        case TRAN_CMD_GETDATETIME:                    // return the current date of DateService
            if( !pxDateService->isInitialized() )
            {
              //we don't have a reliable date to report, so we won't
              dataTrans->setStatus(DATE_DATETIME_UNKNOWN);
              break;
            }
            //convert Datetime to packet
            time_t timeraw=(time_t)pxDateService->getTime();
            struct tm * timeinfo;
            timeinfo=localtime( &timeraw );
            BYTE bfr[512];
            bfr[0]='R';
            bfr[1]='T';
            bfr[2]='C';
            bfr[3]=timeinfo->tm_year;  //years since 1900
            bfr[4]=timeinfo->tm_mon;   //months since January 0-11
            bfr[5]=timeinfo->tm_mday;  //day of the month 1-31
            bfr[6]=timeinfo->tm_hour;  //hours since midnight 0-23
            bfr[7]=timeinfo->tm_min;   //minutes after the hour 0-59
            bfr[8]=timeinfo->tm_sec;   //seconds after the minute 0-61 (tm_sec is generally 0-59. The extra range is to accommodate for leap seconds in certain systems.)

            dataTrans->addDataBfr(bfr, 512, true);         //dataTrans->addDataBfr(bfr, 512, true);
            dataTrans->setStatus(DATE_OK);
            break;
    }

    dataTrans->sendDataAndStatus();         // send all the stuff after handling, if we got any
    Debug::out(LOG_DEBUG, "DateAcsiCommand done.");
}
