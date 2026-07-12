#ifndef ARG_READER_H
#define ARG_READER_H

struct args
{
    char** argv;
    int argc;
    unsigned long page_size, memory_size;
    char** guests;
    int guest_num;
    char** files;
    int file_num;
};

char* get_arg_single(char* argv[], int argc, const char* option);

char* get_arg_single_2_options(char* argv[], int argc, const char* option1, const char* option2);

char** get_arg_array(char* argv[], int argc, const char* option, int* size);

char** get_arg_array_2_options(char* argv[], int argc, const char* option1, const char* option2, int* size);

void parse_args(struct args* args);

#endif