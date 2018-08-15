#ifndef _SCREENCASTACSICOMMAND_H_
#define _SCREENCASTACSICOMMAND_H_

#include "../datatrans.h"
#include "service/screencastservice.h"

class ScreencastAcsiCommand
{
public:
  	ScreencastAcsiCommand(DataTrans *dt, ScreencastService *scs);
  	~ScreencastAcsiCommand();
  	void processCommand(BYTE *command);
private:
    void readScreen();
    void readPalette();
	DWORD get24bits(BYTE *bfr);
  	DataTrans       *dataTrans;
    ScreencastService   *screencastService;
  	BYTE    *cmd;
	BYTE	*dataBuffer; 
};
#endif                
