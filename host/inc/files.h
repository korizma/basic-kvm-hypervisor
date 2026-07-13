#ifndef FILES_H
#define FILES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FUNC_OPEN 1
#define FUNC_CLOSE 2
#define FUNC_READ 3
#define FUNC_WRITE 4
#define FUNC_LSEEK 5

#define O_RD 1
#define O_WR 2
#define O_RDWR 4
#define O_CREATE 8

#define SEEK_SET 1
#define SEEK_END 2

#define READ_STATE_INVALID 1
#define READ_STATE_OPCODE 2
#define READ_STATE_FD 3
#define READ_STATE_SIZE 4
#define READ_STATE_BUFFER 5
#define READ_STATE_FLAG 6
#define READ_STATE_OFFSET 7
#define READ_STATE_FINISH 8
#define READ_STATE_BUFFER_LOC 9

struct vm;

struct file_content
{
    char* path;
    // for copy on write
    bool can_write;
    char* content;
    uint64_t content_size;
    uint64_t cursor;
    bool opened;

    bool read_permission, write_permision;
};

struct file_base
{
    struct file_content** files;
    uint16_t file_num;
};

struct file_operation
{
    uint8_t read_state;

    char* buffer;
    uint32_t buffer_size, buffer_indx;
    uint32_t flags;
    uint32_t function_type;
    uint32_t offset;
    uint32_t fd;

    bool buffer_can_be_freed;
};

void print_file_base_content(const struct file_base* file_base);

int open_initial_files(char** files, uint16_t file_num, struct file_base* file_base);

void create_file_base_using_initial_base(struct file_base *new_file_base, struct file_base* base);

int open_file(struct file_base* file_base, const char* path, uint8_t flags);

int close_file(struct file_base* file_base, int fd);

int read_file(struct file_base* file_base, int fd, char* buffer, size_t size);

int write_file(struct file_base* file_base, int fd, const char* buffer, size_t size);

int file_lseek(struct file_base* file_base, int fd, int64_t offset, uint8_t off_flag);


void advance_state_file_operation(struct file_operation* op, uint32_t next);

void clear_file_operation(struct file_operation *op);

int execute_file_operation(struct vm* v, struct file_operation* op);

#endif /* FILES_H */
