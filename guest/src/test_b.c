#include "test_b.h"

#include "files.h"
#include "io.h"

static void print(const char *message)
{
    while (*message)
        outb(0xE9, *message++);
}

static int strings_equal(const char *left, const char *right, int length)
{
    for (int i = 0; i < length; ++i)
    {
        if (left[i] != right[i])
            return 0;
    }

    return 1;
}

static void finish_with_error(const char *message, int fd)
{
    print(message);
    if (fd >= 0)
        close(fd);
    asm volatile("hlt");
}

void test1_b(void)
{
    static const char path[] = "testb.txt";
    static const char expected[] = "Hello from phase B!";
    char received[sizeof(expected) - 1];
    const int message_size = sizeof(expected) - 1;

    print("Phase B file test: starting\n");

    int fd = open(path, O_CREATE | O_RDWR);
    if (fd < 0)
        finish_with_error("Phase B file test: FAIL (open)\n", -1);

    if (write(fd, expected, message_size) != message_size)
        finish_with_error("Phase B file test: FAIL (write)\n", fd);

    if (lseek(fd, 0, SEEK_SET) != 0)
        finish_with_error("Phase B file test: FAIL (lseek)\n", fd);

    if (read(fd, received, message_size) != message_size)
        finish_with_error("Phase B file test: FAIL (read)\n", fd);

    if (!strings_equal(expected, received, message_size))
        finish_with_error("Phase B file test: FAIL (content mismatch)\n", fd);

    if (close(fd) != 0)
        finish_with_error("Phase B file test: FAIL (close)\n", -1);

    print("Phase B file test: PASS\n");
    asm volatile("hlt");
}
