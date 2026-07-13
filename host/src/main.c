#include "vm.h"
#include "arg_reader.h"
#include "vm_run.h"
#include "files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[])
{
    struct args args;
    args.argc = argc;
    args.argv = argv;

    parse_args(&args);

    if (args.memory_size == 0)
    {
		printf("The program requests a memory size: %s --memory <2 or 4 or 8>\n", argv[0]);
        return 1;
    }

    if (args.page_size == 0)
    {
		printf("The program requests a page size: %s --page <2 or 4>\n", argv[0]);
        return 1;
    }

    if (args.guests == 0)
    {
		printf("The program requests guest programs: %s --guest <list-of-filenames>\n", argv[0]);
        return 1;
    }

	int kvm_fd = open("/dev/kvm", O_RDWR);

	if (kvm_fd < 0) 
    {
		perror("open /dev/kvm");
		return -1;
	}

    struct file_base file_base;
    int files_sig = open_initial_files(args.files, args.file_num, &file_base);

    if (files_sig != 0)
    {
        printf("Failed to open files\n");
        return -1;
    }

    struct buffer buffer;
    buffer.reader_num = args.guest_num-1;
    if (pthread_mutex_init(&buffer.mutex, NULL) != 0)
    {
        perror("mutex init");
        return 1;
    }
    buffer.reading = buffer.writing = false;
    buffer.size = 0;
    buffer.readers_finished = 0;

    pthread_t** threads = (pthread_t**)malloc(sizeof(pthread_t*) * args.guest_num);

    for (int i = 0; i < args.guest_num; i++)
    {
        threads[i] = create_vm_thread(i, kvm_fd, args.guests[i], args.memory_size, args.page_size, &file_base, &buffer);
    }

    for (int i = 0; i < args.guest_num; i++)
    {
        if (threads[i] == 0)
            continue;
        pthread_join(*threads[i], NULL);
    }
    pthread_mutex_destroy(&buffer.mutex);

	return 0;
}
