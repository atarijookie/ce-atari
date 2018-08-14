#ifndef _DATEACSICOMMAND_H_
#define _DATEACSICOMMAND_H_

#include "../datatrans.h"

#include "../service/configservice.h"

#define DATE_CMD_IDENTIFY                    0
#define DATE_CMD_GETDATETIME                 1

#define DATE_OK                              0
#define DATE_ERROR                           2
#define DATE_DATETIME_UNKNOWN                4

class DateAcsiCommand
{
public:
  DateAcsiCommand(DataTrans *dt, ConfigService *ds);
  ~DateAcsiCommand();
  void processCommand(BYTE *command);
private:
  DataTrans       *dataTrans;
  ConfigService   *pxDateService;
  BYTE    *cmd;
};
#endif
