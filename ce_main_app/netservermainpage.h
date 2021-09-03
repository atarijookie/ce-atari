#ifndef _NETSERVERMAINPAGE_H_
#define _NETSERVERMAINPAGE_H_

#include <stdint.h>

class NetServerMainPage {
public:
    static void create(std::string &page, uint8_t *serverIp);
};

#endif
