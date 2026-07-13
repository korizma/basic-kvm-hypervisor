#include "files.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "vm.h"


void advance_state_file_operation(struct file_operation* op, uint32_t next)
{
    switch(op->read_state)
    {
        case READ_STATE_OPCODE:
            op->function_type = next;
            if (next == FUNC_OPEN)
            {
                op->read_state = READ_STATE_SIZE;
            }
            else
            {
                op->read_state = READ_STATE_FD;
            }
            break;

        case READ_STATE_SIZE:
            if (next <= 0)
            {
                op->read_state = READ_STATE_INVALID;
                break;
            }
            op->buffer_size = next;
            if (op->function_type == FUNC_READ)
            {
                op->read_state = READ_STATE_BUFFER_LOC;
            }
            else
            {
                op->buffer = (char*)malloc(op->buffer_size * sizeof(char));
                op->buffer_can_be_freed = true;
                op->read_state = READ_STATE_BUFFER;
            }
            break;

        case READ_STATE_BUFFER:
            op->buffer[op->buffer_indx++] = next;

            if (op->function_type == FUNC_OPEN)
            {
                if (op->buffer_indx == 1 && !isalpha((char)next))
                {
                    op->read_state = READ_STATE_INVALID;
                }
                else if (!(isalnum((char)next) || (char)next == '.' || (char)next == '/'))
                {
                    op->read_state = READ_STATE_INVALID;
                }
            }

            if (op->buffer_indx == op->buffer_size)
            {
                if (op->function_type == FUNC_OPEN)
                {
                    op->read_state = READ_STATE_FLAG;
                }
                else
                {
                    op->read_state = READ_STATE_FINISH;
                }
            }
            break;

        case READ_STATE_FD:
            op->fd = next;
            if (op->function_type == FUNC_LSEEK)
            {
                op->read_state = READ_STATE_OFFSET;
            }
            else if (op->function_type == FUNC_CLOSE)
            {
                op->read_state = READ_STATE_FINISH;
            }
            else if (op->function_type == FUNC_READ || op->function_type == FUNC_WRITE)
            {
                op->read_state = READ_STATE_SIZE;
            }
            break;

        case READ_STATE_OFFSET:
            op->offset = next;
            op->read_state = READ_STATE_FLAG;
            break;

        case READ_STATE_FLAG:
            op->flags = next;
            op->read_state = READ_STATE_FINISH;
            break;

        case READ_STATE_FINISH:
            op->read_state = READ_STATE_INVALID;
            break;
        
        case READ_STATE_BUFFER_LOC:
            op->buffer = (char*) next;
            op->read_state = READ_STATE_FINISH;
            break;

        default:
            op->read_state = READ_STATE_INVALID;
            break;
    }
}

char* file_to_char_array(const char* filename, uint64_t* size)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
        return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    *size = ftell(file);
    if (*size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *data = malloc((size_t)*size);
    if (!data) {
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(data, 1, (size_t)*size, file);
    fclose(file);

    if (read_size != (size_t)*size) {
        free(data);
        return NULL;
    }

    return data;
}

int insert_file_content_to_base(struct file_base* base, struct file_content *content)
{
    base->file_num++;

    struct file_content** temp = realloc(base->files, base->file_num * sizeof(struct file_content*));

    if (temp == NULL)
    {
        perror("realloc");
        return -1;
    }
    
    base->files = temp;
    base->files[base->file_num-1] = content;
    return 0;
}

int open_initial_files(char** files, uint16_t file_num, struct file_base* file_base)
{
    file_base->files = (struct file_content**)malloc(sizeof(struct file_content*) * file_num);
    file_base->file_num = file_num;

    for (int i = 0; i < file_num; i++)
    {
        struct file_content* content = (struct file_content*)malloc(sizeof(struct file_content));
        content->path = files[i];
        content->content = file_to_char_array(files[i], &content->content_size);
        file_base->files[i] = content;

        if (content->content == NULL)
        {
            for (int j = 0; j <= i; j++)
            {
                free(file_base->files[j]);
            }
            free(file_base->files);
            return -1;
        }
    }

    return 0;
}

void create_file_base_using_initial_base(struct file_base *new_file_base, struct file_base* base)
{
    new_file_base->file_num = base->file_num;
    new_file_base->files = (struct file_content**)malloc(sizeof(struct file_content*) * new_file_base->file_num);

    for (int i = 0; i < new_file_base->file_num; i++)
    {
        struct file_content* content = (struct file_content*)malloc(sizeof(struct file_content));

        content->path = base->files[i]->path;

        content->content = base->files[i]->content;
        content->content_size = base->files[i]->content_size;

        content->can_write = false;
        content->cursor = 0;
        content->opened = false;

        new_file_base->files[i] = content;
    }
}

int open_file(struct file_base* file_base, const char* path, uint8_t flags)
{
    bool only_read = (flags & O_RD) != 0;
    bool only_write = (flags & O_WR) != 0;
    bool read_write = (flags & O_RDWR) != 0;
    bool create = (flags & O_CREATE) != 0;

    if ((only_read & only_write) || (only_read & read_write) || (only_write & read_write))
        return -1;

    int indx = -1;
    for (int i = 0; i < file_base->file_num; i++)
    {
        if (strcmp(path, file_base->files[i]->path) == 0)
        {
            indx = i;
            break;
        }
    }

    // file not found and flag create not present
    if (indx == -1 && !create)
        return -1;
    
    struct file_content* curr_file;
    
    if (indx == -1)
    {
        curr_file = (struct file_content*)malloc(sizeof(struct file_content));
        curr_file->path = strdup(path);
        curr_file->content = (char*)malloc(sizeof(char));
        curr_file->content_size = 0;
        curr_file->opened = false;
        curr_file->can_write = true;
        if (curr_file->content == NULL)
        {
            free(curr_file->path);
            free(curr_file);
            return -1;
        }

        indx = file_base->file_num;
        insert_file_content_to_base(file_base, curr_file);
    }
    else
    {
        curr_file = file_base->files[indx];
    }

    if (curr_file->opened)
        return -1;

    if (!curr_file->can_write && (only_write || read_write))
    {
        char* content_copy = (char*)malloc(sizeof(char) * curr_file->content_size);
        for (int i = 0; i < curr_file->content_size; i++)
            content_copy[i] = curr_file->content[i];

        curr_file->content = content_copy;
        curr_file->can_write = true;
    }

    curr_file->cursor = 0;
    curr_file->read_permission = read_write || only_read;
    curr_file->write_permision = read_write || only_write;
    curr_file->opened = true;

    return indx;
}

int close_file(struct file_base* file_base, int fd)
{
    if (fd < 0 || fd >= file_base->file_num)
        return -1;

    file_base->files[fd]->opened = false;

    return 0;
}

int read_file(struct file_base* file_base, int fd, char* buffer, size_t size)
{
    if (fd < 0 || fd >= file_base->file_num)
        return -1;

    if (buffer == 0)
        return -1;

    if (!file_base->files[fd]->opened)
        return -1;

    if (!file_base->files[fd]->read_permission)
        return -1;

    for (int i = 0; i < size; i++)
    {
        buffer[i] = file_base->files[fd]->content[i + file_base->files[fd]->cursor];
    }
    file_base->files[fd]->cursor += size;

    return size;
}

int write_file(struct file_base* file_base, int fd, const char* buffer, size_t size)
{
    if (fd < 0 || fd >= file_base->file_num)
        return -1;

    if (buffer == 0)
        return -1;

    if (!file_base->files[fd]->opened)
        return -1;

    if (!file_base->files[fd]->write_permision)
        return -1;

    if (file_base->files[fd]->cursor + size > file_base->files[fd]->content_size)
    {
        char* temp = (char*)realloc(file_base->files[fd]->content, sizeof(char) * (file_base->files[fd]->content_size + size));
        if (temp == NULL)
            return -1;

        file_base->files[fd]->content = temp;
        file_base->files[fd]->content_size += size;
    }

    for (int i = 0; i < size; i++)
    {
        file_base->files[fd]->content[i + file_base->files[fd]->cursor] = buffer[i];
    }
    file_base->files[fd]->cursor += size;

    return size;
}

int file_lseek(struct file_base* file_base, int fd, int64_t offset, uint8_t off_flag)
{
    bool seek_set = (off_flag & 1) != 0;
    bool seek_end = (off_flag & 2) != 0;

    if (seek_end == seek_set)
        return -1;

    if (fd < 0 || fd >= file_base->file_num)
        return -1;

    if (!file_base->files[fd]->opened)
        return -1;

    if (seek_end)
    {
        file_base->files[fd]->cursor = file_base->files[fd]->content_size;
        return file_base->files[fd]->cursor;
    }
    else 
    {
        if (offset < 0 || (uint64_t)offset >= file_base->files[fd]->content_size)
            return -1;

        file_base->files[fd]->cursor = offset;
        return offset;
    }
}

void clear_file_operation(struct file_operation *op)
{
    if (op->buffer_can_be_freed)
    {
        free(op->buffer);
    }
    op->buffer = 0;
    op->buffer_can_be_freed = false;
    op->buffer_indx = 0;
    op->buffer_size = 0;
    op->fd = 0;
    op->flags = 0;
    op->function_type = 0;
    op->offset = 0;
    op->read_state = READ_STATE_OPCODE;
}

int execute_file_operation(struct vm* v, struct file_operation* op)
{   
    if (op->read_state != READ_STATE_FINISH)
        return -1;

    if (op->function_type == FUNC_READ )
    {
        char* buffer = op->buffer;
        uint64_t buffer_start = (uint64_t)buffer;
        uint64_t buffer_end = buffer_start + op->buffer_size;

        if (buffer_start < GUEST_START_ADDR || buffer_start >= GUEST_START_ADDR + v->mem_size || buffer_end > GUEST_START_ADDR + v->mem_size)
            return -1;
        op->buffer = v->mem + buffer_start;
    }

    switch (op->function_type)
    {
        case FUNC_CLOSE:
            return close_file(&v->file_base, op->fd);

        case FUNC_LSEEK:
            return file_lseek(&v->file_base, op->fd, op->offset, op->flags);

        case FUNC_OPEN:
            char* temp = (char*) realloc(op->buffer, sizeof(char) * (op->buffer_size + 1));

            if (temp == NULL)
                return -1;

            op->buffer = temp;
            op->buffer[op->buffer_size] = '\0';

            return open_file(&v->file_base, op->buffer, op->flags);

        case FUNC_READ:
            return read_file(&v->file_base, op->fd, op->buffer, op->buffer_size);

        case FUNC_WRITE:
            return write_file(&v->file_base, op->fd, op->buffer, op->buffer_size);

    }
    return -1;
}
