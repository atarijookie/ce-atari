#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <string.h>
#include <iostream>
#include <ctime>

#include "configservice.h"
#include "settings.h"
#include "debug.h"

ConfigService::ConfigService():iInitState(INIT_NONE)
{
}

//get NTP time and set system time accordingly
void ConfigService::start()
{
  iInitState=INIT_OK;
  Debug::out(LOG_DEBUG, "DateService: init done.");
}

void ConfigService::stop()
{
}

//return system time with correction value
long ConfigService::getTime()
{
    Settings s;
    float   utcOffset;
    utcOffset   = s.getFloat("TIME_UTC_OFFSET", 0);

    int iSecondsOffset = (int) (utcOffset * 10.0 ) * 60*60;

	time_t timenow      = time(NULL) + iSecondsOffset;              // get time with offset
	return timenow;
}

std::string ConfigService::getTimeString()
{

	time_t tv=(time_t)getTime();
	std::tm * ptm = std::localtime(&tv);
	char buffer[32];
	// Format: Mo, 15.06.2009 20:20:00
	std::strftime(buffer, 32, "%a, %d.%m.%Y %H:%M:%S", ptm);
	return std::string(buffer);
}

bool ConfigService::isInitialized()
{
  return getInitState()==INIT_OK;
}

int ConfigService::getInitState()
{
  return iInitState;
}
