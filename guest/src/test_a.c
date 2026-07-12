#include "test_a.h"
#include "io.h"

static void print(const char *message)
{
    while (*message)
        outb(0xE9, *message++);
}

void test1_a(void)
{
    print("Write one byte to port 0xE9...\n");
    outb(0xE9, 'A');
    outb(0xE9, '\n');

    print("Read one byte from port 0xE9 and echo it: ");
    uint8_t value = inb(0xE9);
    outb(0xE9, value);
    outb(0xE9, '\n');

    asm volatile("hlt");
}
