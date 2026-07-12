#include "test_a.h"

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void)
{
    test1_a();
}
