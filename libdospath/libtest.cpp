#include <iostream>
#include <map>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>

#include "libexports.h"

int main(int argc, char**argv)
{
    ldp_setParam(0, 1);     // enable logging

    system("rm -rf /tmp/test");
    system("mkdir -p /tmp/test");
    system("touch /tmp/test/short.f");                  // short filename
    system("touch '/tmp/test/Long FileName 1.extension'");  // long filename

    std::string lp;
    printf("\n\nshortToLongPath -- 1st run\n");
    ldp_shortToLongPath(std::string("/tmp/test"), lp, false);                // first time - will create shorteners, feed them with dir content
    assert(lp == "/tmp/test");

    printf("\n\nshortToLongPath -- 2nd run\n");
    ldp_shortToLongPath(std::string("/tmp/test"), lp, false);                // second time - will use existing shorteners
    assert(lp == "/tmp/test");

    printf("\n\nshortToLongPath -- long filename is short\n");
    ldp_shortToLongPath(std::string("/tmp/test\\SHORT.F"), lp, false);      // test short filename 
    assert(lp == "/tmp/test/short.f");

    printf("\n\nshortToLongPath -- long filename is long\n");
    ldp_shortToLongPath(std::string("/tmp/test\\LONG_F~1.EXT"), lp, false); // test longer filename
    assert(lp == "/tmp/test/Long FileName 1.extension");

    // create another long file, see if we can miss it without refresh and hit it with refresh
    system("touch '/tmp/test/Long FileName 2.extension'");

    printf("\n\nshortToLongPath -- cache-miss without refresh\n");
    ldp_shortToLongPath(std::string("/tmp/test\\LONG_F~2.EXT"), lp, false); // NO refresh - will miss cache
    assert(lp == "/tmp/test/LONG_F~2.EXT");     // on failed translation the short filename will be also in the long version - this is expected on file / dir create

    printf("\n\nshortToLongPath -- cache-miss with refresh, then cache-hit\n");
    ldp_shortToLongPath(std::string("/tmp/test\\LONG_F~2.EXT"), lp, true);  // WITH refresh - will hit cache
    assert(lp == "/tmp/test/Long FileName 2.extension");

    printf("\n\nclean-up\n");
    ldp_cleanup();

    printf("\n\nC++ tests (on private methods)\n");
    ldp_runCppTests();

    return 0;
}
