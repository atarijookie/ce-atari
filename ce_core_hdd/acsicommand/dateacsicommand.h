#ifndef _DATEACSICOMMAND_H_
#define _DATEACSICOMMAND_H_

#include "../acsidatatrans.h"

#define DATE_CMD_IDENTIFY                    0
#define DATE_CMD_GETDATETIME                 1

#define DATE_OK                              0
#define DATE_ERROR                           2
#define DATE_DATETIME_UNKNOWN                4

class DateAcsiCommand
{
public:
  DateAcsiCommand(AcsiDataTrans *dt);
  ~DateAcsiCommand();
  void processCommand(uint8_t *command);
private:
  AcsiDataTrans       *dataTrans;
  uint8_t    *cmd;
};
#endif
