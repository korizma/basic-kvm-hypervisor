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

static int append_decimal(char *buffer, uint8_t value)
{
    char reversed[3];
    int digit_count = 0;

    do
    {
        reversed[digit_count++] = '0' + value % 10;
        value /= 10;
    } while (value != 0);

    for (int i = 0; i < digit_count; ++i)
        buffer[i] = reversed[digit_count - i - 1];

    return digit_count;
}

void test1_b(void)
{
    static const char path[] = "test_files/test2_b.txt";
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

void test2_b(void)
{
    enum { READ_BUFFER_SIZE = 256 };
    static const char path[] = "testfiles/testb.txt";
    static const char prefix[] = "Special Number ";
    char contents[sizeof(prefix) - 1 + 3];
    char received[READ_BUFFER_SIZE];
    int contents_size = sizeof(prefix) - 1;

    print("Phase B test 2: waiting for a number\n");
    uint8_t number = inb(0xE9);

    for (int i = 0; i < contents_size; ++i)
        contents[i] = prefix[i];
    contents_size += append_decimal(contents + contents_size, number);

    int fd = open(path, O_CREATE | O_RDWR);
    if (fd < 0)
        finish_with_error("Phase B test 2: FAIL (open)\n", -1);
    
    int original_size = lseek(fd, 0, SEEK_END);
    if (original_size < 0)
        finish_with_error("Phase B test 2: FAIL (seek to end)\n", fd);

    if (write(fd, contents, contents_size) != contents_size)
        finish_with_error("Phase B test 2: FAIL (write)\n", fd);

    int final_size = lseek(fd, 0, SEEK_END);
    if (final_size != original_size + contents_size)
        finish_with_error("Phase B test 2: FAIL (file size)\n", fd);

    if (final_size > READ_BUFFER_SIZE)
        finish_with_error("Phase B test 2: FAIL (file too large)\n", fd);

    if (lseek(fd, 0, SEEK_SET) != 0)
        finish_with_error("Phase B test 2: FAIL (seek to start)\n", fd);

    if (read(fd, received, final_size) != final_size)
        finish_with_error("Phase B test 2: FAIL (read)\n", fd);

    if (close(fd) != 0)
        finish_with_error("Phase B test 2: FAIL (close)\n", -1);

    for (int i = 0; i < final_size; ++i)
        outb(0xE9, received[i]);
    outb(0xE9, '\n');

    asm volatile("hlt");
}
