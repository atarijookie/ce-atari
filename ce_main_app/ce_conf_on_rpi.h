#ifndef _CE_CONF_ON_RPI_H_
#define _CE_CONF_ON_RPI_H_ 

#define FIFO_WEB_PATH1  "/tmp/ce_conf_fifo_web_1"
#define FIFO_WEB_PATH2  "/tmp/ce_conf_fifo_web_2"

#define FIFO_TERM_PATH1  "/tmp/ce_conf_fifo_term_1"
#define FIFO_TERM_PATH2  "/tmp/ce_conf_fifo_term_2"

void ce_conf_mainLoop(void);
void ce_conf_createFifos(void);

#endif
