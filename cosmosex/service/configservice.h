#ifndef _DATESERVICE_H_
#define _DATESERVICE_H_

#include <string>

class ConfigService
{
public:
  ConfigService();
	void start();
	void stop();
  //get Time including TimeOffset
  long getTime();
  std::string getTimeString();
  bool isInitialized();
  int getInitState();
private:
  long lTime;   // the time -- This is a time_t sort of
  long lTimeOffset; //Time Offset to handle current timezone
  //keep track where initialization might fail to be able to give the user a hint
  enum eInitState {INIT_NONE=0, INIT_NTP_FAILED=1, INIT_DATE_NOT_SET=2, INIT_OK=3};
  int iInitState;
};
#endif
