#ifndef __LIBEXPORTS_H__
#define __LIBEXPORTS_H__

#include <iostream>
#include <stdint.h>
#include <sys/types.h>

extern "C" {
    void ldp_shortToLongPath(const std::string &shortPath, std::string &longPath);
    void ldp_cleanup(void);
}

#endif
