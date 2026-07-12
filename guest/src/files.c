#include "files.h"

#include <stdint.h>

#define FILE_PORT 0x0278

#define FUNC_OPEN  1
#define FUNC_CLOSE 2
#define FUNC_READ  3
#define FUNC_WRITE 4
#define FUNC_LSEEK 5

static inline void file_out(uint32_t value)
{
    asm volatile("outl %0, %1" : : "a"(value), "d"((uint16_t)FILE_PORT) : "memory");
}

static inline int file_result(void)
{
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "d"((uint16_t)FILE_PORT) : "memory");
    return (int)value;
}

static uint32_t path_length(const char *path)
{
    uint32_t length = 0;

    while (path[length] != '\0')
        ++length;

    return length;
}

int open(const char *path, int flags)
{
    uint32_t length;

    if (path == 0)
        return -1;

    length = path_length(path);
    if (length == 0)
        return -1;

    file_out(FUNC_OPEN);
    file_out(length);
    for (uint32_t i = 0; i < length; ++i)
        file_out((uint8_t)path[i]);
    file_out((uint32_t)flags);

    return file_result();
}

int close(int fd)
{
    file_out(FUNC_CLOSE);
    file_out((uint32_t)fd);
    return file_result();
}

int read(int fd, char *buf, int count)
{
    if (buf == 0 || count <= 0)
        return -1;

    file_out(FUNC_READ);
    file_out((uint32_t)fd);
    file_out((uint32_t)count);
    file_out((uint32_t)(uintptr_t)buf);
    return file_result();
}

int write(int fd, const char *buf, int count)
{
    if (buf == 0 || count <= 0)
        return -1;

    file_out(FUNC_WRITE);
    file_out((uint32_t)fd);
    file_out((uint32_t)count);
    for (int i = 0; i < count; ++i)
        file_out((uint8_t)buf[i]);
    return file_result();
}

int lseek(int fd, const int offset, int off_flag)
{
    file_out(FUNC_LSEEK);
    file_out((uint32_t)fd);
    file_out((uint32_t)offset);
    file_out((uint32_t)off_flag);
    return file_result();
}
