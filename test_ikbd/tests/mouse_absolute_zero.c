#include <mint/osbind.h> 
#include "test.h"
#include "../helper/commands.h"
#include "mouse_absolute_zero.h"

void test_mouse_absolute_zero_init()
{
	(void) Cconws("     test_mouse_absolute_zero ");
	ikbd_disable_irq(); 	//disable KBD IRQ so TOS doesn't recieve IKBD return values
	ikbd_reset();
}

BYTE test_mouse_absolute_zero_run()
{
	return FALSE;
}

void test_mouse_absolute_zero_teardown()
{
}

TTestIf test_mouse_absolute_zero={&test_mouse_absolute_zero_init, &test_mouse_absolute_zero_run, &test_mouse_absolute_zero_teardown};
