#include "test_c.h"

#include "files.h"
#include "interrupts.h"
#include "io.h"

static const char shared_path[] = "testfiles/testc.txt";
static const char local_path[] = "phasec.txt";
static const uint8_t expected_payload[] = "Phase C shared-file payload.\n";
static const uint8_t failure_payload[] = "!";

static void print(const char *message)
{
    while (*message != '\0')
        outb(0xE9, (uint8_t)*message++);
}

static int buffers_equal(const uint8_t *left, const uint8_t *right, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
    {
        if (left[i] != right[i])
            return 0;
    }

    return 1;
}

int test_c_prepare_writer(void)
{
    uint8_t file_contents[BUFFER_SIZE];
    const uint32_t expected_size = sizeof(expected_payload) - 1;
    int fd = open(shared_path, O_RD);

    if (fd < 0)
        goto fail;

    int file_size = lseek(fd, 0, SEEK_END);
    if (file_size <= 0 || file_size > BUFFER_SIZE || (uint32_t)file_size != expected_size)
        goto fail_close;

    if (lseek(fd, 0, SEEK_SET) != 0)
        goto fail_close;

    if (read(fd, (char *)file_contents, file_size) != file_size)
        goto fail_close;

    if (!buffers_equal(file_contents, expected_payload, expected_size))
        goto fail_close;

    if (close(fd) != 0)
        goto fail;

    interrupt_set_write_buffer(file_contents, expected_size);
    return 1;

fail_close:
    close(fd);
fail:
    interrupt_set_write_buffer(failure_payload, sizeof(failure_payload) - 1);
    return 0;
}

void test_c_reader(void)
{
    uint8_t received[BUFFER_SIZE];
    uint8_t stored[BUFFER_SIZE];
    const uint32_t expected_size = sizeof(expected_payload) - 1;
    int fd = -1;
    int file_operation_succeeded = 1;
    int passed = 1;

    while (!interrupt_read_ready())
        asm volatile("hlt");

    uint32_t size = interrupt_copy_read_buffer(received, sizeof(received));
    if (size != expected_size || !buffers_equal(received, expected_payload, expected_size))
        passed = 0;

    fd = open(local_path, O_CREATE | O_RDWR);
    if (fd < 0)
        file_operation_succeeded = 0;

    if (file_operation_succeeded &&
        (size == 0 || write(fd, (const char *)received, size) != (int)size))
        file_operation_succeeded = 0;

    if (file_operation_succeeded && lseek(fd, 0, SEEK_SET) != 0)
        file_operation_succeeded = 0;

    if (file_operation_succeeded && read(fd, (char *)stored, size) != (int)size)
        file_operation_succeeded = 0;

    if (file_operation_succeeded && !buffers_equal(stored, received, size))
        file_operation_succeeded = 0;

    if (fd >= 0 && close(fd) != 0)
        file_operation_succeeded = 0;

    if (!file_operation_succeeded)
        passed = 0;

    if (passed)
    {
        outb(0xE9, '#');
        print("Phase C reader: PASS\n");
    }
    else
    {
        outb(0xE9, '!');
        print("Phase C reader: FAIL\n");
    }
}

void test_c_writer(int preparation_succeeded)
{
    while (!interrupt_write_finished())
        asm volatile("hlt");

    if (preparation_succeeded && interrupt_write_succeeded())
    {
        outb(0xE9, '@');
        print("Phase C writer: PASS\n");
    }
    else
    {
        outb(0xE9, '!');
        print("Phase C writer: FAIL\n");
    }
}
