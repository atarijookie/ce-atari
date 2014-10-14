#ifndef _PORT_H_
#define _PORT_H_

void  init_ports    (void);
int16 on_port       (char *port_name);
void  off_port      (char *port_name);
int16 query_port    (char *port_name);
int16 cntrl_port    (char *port_name, uint32 argument, int16 code);
PORT *search_port   (char *port_name);
int16 my_set_state  (PORT *port, int16 state);
int16 my_cntrl      (PORT *port, uint32 argument, int16 code);

#endif

