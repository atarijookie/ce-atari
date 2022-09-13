#ifndef __LIBEXPORTS_H__
#define __LIBEXPORTS_H__

#include <iostream>
#include <stdint.h>
#include <sys/types.h>
#include "defs.h"

extern "C" {
    void ldp_setParam(int paramNo, int paramVal);
    void ldp_shortToLongPath(const std::string& shortPath, std::string& longPath, bool refreshOnMiss);
    bool ldp_findFirstAndNext(SearchParams& sp, DiskItem& di);
    void ldp_diskItemToAtariFindStorageItem(DiskItem& di, uint8_t* buf);
    void ldp_cleanup(void);
    void ldp_runCppTests(void);
}

#endif
