#ifndef _CE_CONF_ON_RPI_H_
#define _CE_CONF_ON_RPI_H_ 

#define FIFO_PATH1  "/tmp/ce_conf_fifo_1"
#define FIFO_PATH2  "/tmp/ce_conf_fifo_2"

extern int ce_conf_fd1;
extern int ce_conf_fd2;

void ce_conf_mainLoop(void);
void ce_conf_createFifos(void);

#endif
