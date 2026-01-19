#ifndef _SCREENCASTACSICOMMAND_H_
#define _SCREENCASTACSICOMMAND_H_

#include <semaphore.h>
#include "../acsidatatrans.h"

#define SCREENCAST_BUFFER_SIZE      (64 * 1024)

class ScreencastAcsiCommand
{
public:
  	ScreencastAcsiCommand(AcsiDataTrans *dt);
  	~ScreencastAcsiCommand();
  	void processCommand(uint8_t *command);
private:
    void readScreen(void);
    void readPalette(void);
  	AcsiDataTrans *dataTrans;
  	uint8_t *cmd;
	uint8_t	*dataBuffer;

	int sharedMemFd;
	sem_t *sharedMemSemaphore;
	uint8_t *sharedMemPointer;

	void sharedMemoryOpen(void);
	void sharedMemoryClose(void);
};
#endif
