#ifndef _TEST_H_
#define _TEST_H_
 
#include "../global.h"

typedef void  (*TTestInit) (void);
typedef BYTE  (*TRun) (void);
typedef void  (*TTearDown) (void);

typedef struct {
	TTestInit	init;	
	TRun		run;
	TTearDown 	tearDown;
} TTestIf;

#define TEST_FAIL_REASON( reason ) (void)Cconws(reason)
#define ASSERT_EQUAL( result, value, reason ) if( result!=value ){ TEST_FAIL_REASON(reason); return FALSE; }
/* NOTE: the BYTE cast is necessary, as TRUE is an int and GCC doesn't cast result correctly (compares garbage in high word) */
#define ASSERT_SUCCESS( result, reason ) ASSERT_EQUAL( (BYTE)result, (BYTE)TRUE, reason )

#endif
