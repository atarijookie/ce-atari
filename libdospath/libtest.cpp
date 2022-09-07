#include <iostream>
#include <map>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "libexports.h"

int main(int argc, char**argv)
{
    ldp_setParam(0, 1);     // enable logging

    mkdir("/tmp/test", S_IRUSR | S_IRGRP | S_IROTH);
    std::string sp = "/tmp/test";
    std::string lp;

    printf("shortToLongPath -- 1st run\n\n");
    ldp_shortToLongPath(sp, lp);                // first time
    printf("output: %s\n", lp.c_str());

    printf("shortToLongPath -- 2nd run\n\n");
    ldp_shortToLongPath(sp, lp);                // second time
    printf("output: %s\n", lp.c_str());

    ldp_cleanup();

    ldp_runCppTests();

    return 0;
}
