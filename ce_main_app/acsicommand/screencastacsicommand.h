#ifndef _SCREENCASTACSICOMMAND_H_
#define _SCREENCASTACSICOMMAND_H_

#include "../acsidatatrans.h" 
#include "service/screencastservice.h"

class ScreencastAcsiCommand
{
public:
  	ScreencastAcsiCommand(AcsiDataTrans *dt, ScreencastService *scs);
  	~ScreencastAcsiCommand();
  	void processCommand(uint8_t *command);
private:
    void readScreen();
    void readPalette();
	uint32_t get24bits(uint8_t *bfr);
  	AcsiDataTrans       *dataTrans;
    ScreencastService   *screencastService;
  	uint8_t    *cmd;
	uint8_t	*dataBuffer; 
};
#endif                