#ifndef _SCREENCASTACSICOMMAND_H_
#define _SCREENCASTACSICOMMAND_H_

#include "../acsidatatrans.h" 
#include "service/screencastservice.h"

class ScreencastAcsiCommand
{
public:
  	ScreencastAcsiCommand(AcsiDataTrans *dt, ScreencastService *scs);
  	~ScreencastAcsiCommand();
  	void processCommand(BYTE *command);
private:
    void readScreen();
    void readPalette();
	DWORD get24bits(BYTE *bfr);
  	AcsiDataTrans       *dataTrans;
    ScreencastService   *screencastService;
  	BYTE    *cmd;
	BYTE	*dataBuffer; 
};
#endif                