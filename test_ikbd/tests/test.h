#ifndef _TEST_H_
#define _TEST_H_
 
#include "../../libacsiscsi/global.h"

typedef void  (*TTestInit) (void);
typedef uint8_t  (*TRun) (void);
typedef void  (*TTearDown) (void);

typedef struct {
	TTestInit	init;	
	TRun		run;
	TTearDown 	tearDown;
} TTestIf;

extern int testLine;

#define TEST_FAIL_REASON( reason ) { VT52_Goto_pos(25, testLine); (void)Cconws(reason); }
#define ASSERT_EQUAL( result, value, reason ) if( result!=value ){ TEST_FAIL_REASON(reason); return FALSE; }
/* NOTE: the uint8_t cast is necessary, as TRUE is an int and GCC doesn't cast result correctly (compares garbage in high uint16_t) */
#define ASSERT_SUCCESS( result, reason ) ASSERT_EQUAL( (uint8_t)result, (uint8_t)TRUE, reason )

#endif
