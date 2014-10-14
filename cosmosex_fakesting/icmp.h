#ifndef _ICMP_H_
#define _ICMP_H_

int16 ICMP_send (uint32 dest, uint8 type, uint8 code, void *data, uint16 dat_length);
int16 ICMP_handler (int16 (* handler) (IP_DGRAM *), int16 flag);
void  ICMP_discard (IP_DGRAM *dgram);

#endif
