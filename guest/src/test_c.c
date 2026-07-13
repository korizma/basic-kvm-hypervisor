#include "test_c.h"

#include "interrupts.h"
#include "io.h"

static const uint8_t test_message[] = "Hello C";

static void print(const char *message)
{
    while (*message != '\0')
        outb(0xE9, (uint8_t)*message++);
}

void test_c_prepare(void)
{
    interrupt_set_write_buffer(test_message, sizeof(test_message) - 1);
}

void test_c_reader(void)
{
    uint8_t received[sizeof(test_message) - 1];

    while (!interrupt_read_ready())
        asm volatile("hlt");

    uint32_t size = interrupt_copy_read_buffer(received, sizeof(received));
    if (size != sizeof(test_message) - 1)
    {
        print("Phase C reader: FAIL (size)\n");
        return;
    }

    for (uint32_t i = 0; i < size; ++i)
    {
        if (received[i] != test_message[i])
        {
            print("Phase C reader: FAIL (data)\n");
            return;
        }
    }

    print("Phase C reader: PASS\n");
}

void test_c_writer(void)
{
    while (!interrupt_write_finished())
        asm volatile("hlt");

    if (interrupt_write_succeeded())
        print("Phase C writer: PASS\n");
    else
        print("Phase C writer: FAIL (size)\n");
}
