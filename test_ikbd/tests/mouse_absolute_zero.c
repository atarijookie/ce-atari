#include <mint/osbind.h> 
#include "test.h"
#include "mouse_absolute_zero.h"

void test_mouse_absolute_zero_init()
{
	(void) Cconws("     test_mouse_absolute_zero ");
}

BYTE test_mouse_absolute_zero_run()
{
	return FALSE;
}

TTestIf test_mouse_absolute_zero={&test_mouse_absolute_zero_init, &test_mouse_absolute_zero_run, 0};
