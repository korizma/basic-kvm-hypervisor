#include "arg_reader.h"
#include <string.h>
#include <stdlib.h>
#include "vm.h"


int find_string_in_array(char* list[], int size, const char* s)
{
    for (int i = 0; i < size; i++)
    {
        if (strcmp(list[i], s) == 0)
            return i;
    }
    return -1;
}

char* get_arg_single(char* argv[], int argc, const char* option)
{
    int i = find_string_in_array(argv, argc, option);

    if (i == -1 || i + 1 == argc || i == 0)
        return 0;

    return argv[i+1];
}

char** get_arg_array(char* argv[], int argc, const char* option, int* size)
{
    int i = find_string_in_array(argv, argc, option);

    if (i == -1 || i + 1 == argc || i == 0)
        return 0;

    int arg_num = 0;
    for (int j = i+1; j < argc; j++)
    {
        if (argv[j][0] == '-')
            break;

        arg_num++;
    }
    *size = arg_num;

    char** arr = (char**)malloc(arg_num * sizeof(char*));
    for (int j = 0; j < arg_num; j++)
    {
        arr[j] = argv[i + j + 1]; 
    }

    return arr;
}

char* get_arg_single_2_options(char* argv[], int argc, const char* option1, const char* option2)
{
    char* opt1 = get_arg_single(argv, argc, option1);
    char* opt2 = get_arg_single(argv, argc, option2);

    if (opt1)
        return opt1;
    if (opt2)
        return opt2;
    return 0;
}


char** get_arg_array_2_options(char* argv[], int argc, const char* option1, const char* option2, int* size)
{
    char** opt1 = get_arg_array(argv, argc, option1, size);
    char** opt2 = get_arg_array(argv, argc, option2, size);

    if (opt1)
    {
        if (opt2)
            free(opt2);
        return opt1;
    }
    if (opt2)
        return opt2;
    return 0;
}



void parse_args(char* argv[], int argc, unsigned long* page_size, unsigned long* memory_size, char*** guests, int* guest_num)
{
    char* phys_mem_string = get_arg_single_2_options(argv, argc, "-m", "--memory");
    char* page_size_string = get_arg_single_2_options(argv, argc, "-p", "--page");

    *guests = get_arg_array_2_options(argv, argc, "-g", "--guest", guest_num);

    if (phys_mem_string == 0)
        phys_mem_string = "a";
    
    if (page_size_string == 0)
        page_size_string = "a";

    switch (phys_mem_string[0])
    {
        case '2':
            *memory_size = PHYS_MEM_SIZE_2MB;
            break;
        case '4':
            *memory_size = PHYS_MEM_SIZE_4MB;
            break;
        case '8':
            *memory_size = PHYS_MEM_SIZE_8MB;
            break;
        default:
            *memory_size = 0;
    }

    switch (page_size_string[0])
    {
        case '2':
            *page_size = PAGE_SIZE_2MB;
            break;
        case '4':
            *page_size = PAGE_SIZE_4KB;
            break;
        default:
            *page_size = 0;
    }
}
