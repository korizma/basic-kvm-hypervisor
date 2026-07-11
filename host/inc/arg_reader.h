#ifndef ARG_READER_H
#define ARG_READER_H

char* get_arg_single(char* argv[], int argc, const char* option);

char* get_arg_single_2_options(char* argv[], int argc, const char* option1, const char* option2);

char** get_arg_array(char* argv[], int argc, const char* option, int* size);

char** get_arg_array_2_options(char* argv[], int argc, const char* option1, const char* option2, int* size);

void parse_args(char* argv[], int argc, unsigned long* page_size, unsigned long* memory_size, char*** guests, int* guest_num);

#endif