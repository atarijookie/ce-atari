#ifndef __LIBCPPTESTS_H__
#define __LIBCPPTESTS_H__

#include <iostream>
#include <stdint.h>
#include <sys/types.h>

class TestClass 
{
public:
    void runTests(void);
    
    void splitPathAndFilename(void);
    void testMerge(void);
    void longToShortAndBack(void);
    void testFeedingShortener(void);
    void testNamesCountLimiting(void);
};

#endif
