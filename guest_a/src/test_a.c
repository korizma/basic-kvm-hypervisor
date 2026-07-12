#include "test_a.h"
#include "io.h"

#define TEST_PORT 0xE9
#define ITERATION_COUNT 30
#define E_OUTPUT_ITERATION 30

void test1_a(void)
{
    for (int i = 1; i <= ITERATION_COUNT; ++i) {
        uint8_t value = inb(TEST_PORT);

        if (i == E_OUTPUT_ITERATION)
            value = 'e';

        outb(TEST_PORT, value);
    }

    asm volatile("hlt");
}
