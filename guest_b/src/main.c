#include "test_b.h"


void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void)
{
    test2_b();
}
