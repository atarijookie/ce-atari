#ifndef _NETADAPTER_H_
#define _NETADAPTER_H_

#include <string>

#include "../acsidatatrans.h"
#include "../settings.h"
#include "../datatypes.h"
#include "../isettingsuser.h"

class NetAdapter: public ISettingsUser
{
public:
    NetAdapter(void);
    virtual ~NetAdapter();

    void reloadSettings(int type);
    void setAcsiDataTrans(AcsiDataTrans *dt);

    void processCommand(BYTE *command);
	
private:
    AcsiDataTrans   *dataTrans;
    BYTE            *cmd;

    void loadSettings(void);
};

#endif
