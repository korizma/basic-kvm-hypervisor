#include "arg_reader.h"
#include <string.h>
#include <stdlib.h>


int find_string_in_array(char* list[], int size, const char* s)
{
    for (int i = 0; i < size; i++)
    {
        if (strcmp(list[i], s) == 0)
            return i;
    }
    return 0;
}

char* get_arg_single(char* argv[], int argc, const char* option)
{
    int i = find_string_in_array(argv, argc, option);

    if (i == -1 || i + 1 == argc || i == 0)
        return 0;

    return argv[i];
}

char** get_arg_array(char* argv[], int argc, const char* option)
{
    int i = find_string_in_array(argv, argc, option);

    if (i == -1 || i + 1 == argc || i == 0)
        return 0;

    int arg_num = 0;
    for (int j = i+1; j < argc; j++)
    {
        if (argv[i][0] == '-')
            break;

        arg_num++;
    }

    char** arr = (char**)malloc(arg_num * sizeof(char*));
    for (int j = 0; j < arg_num; j++)
    {
        arr[j] = argv[i + j]; 
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


char** get_arg_array_2_options(char* argv[], int argc, const char* option1, const char* option2)
{
    char** opt1 = get_arg_array(argv, argc, option1);
    char** opt2 = get_arg_array(argv, argc, option2);

    if (opt1)
        return opt1;
    if (opt2)
        return opt2;
    return 0;
}

