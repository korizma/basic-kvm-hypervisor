#include "vm.h"
#include "arg_reader.h"
#include "vm_run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int main(int argc, char *argv[])
{
	struct vm v;
	struct kvm_sregs sregs;
	struct kvm_regs regs;
	int stop = 0;
	int ret = 0;
	int irq_pending = IRQ_COUNT;

    int phys_mem_size = 0;
    int page_size = 0;

    char** guests;
    int guest_num;

    parse_args(argv, argc, &page_size, &phys_mem_size, &guests, &guest_num);

    if (phys_mem_size == 0)
    {
		printf("The program requests a memory size: %s --memory <2 or 4 or 8>\n", argv[0]);
        return 1;
    }

    if (page_size == 0)
    {
		printf("The program requests a page size: %s --page <2 or 4>\n", argv[0]);
        return 1;
    }

    if (guests == 0)
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

    pthread_t** threads = (pthread_t**)malloc(sizeof(pthread_t*) * guest_num);

    for (int i = 0; i < guest_num; i++)
    {
        threads[i] = create_vm_thread(kvm_fd, guests[i], phys_mem_size, page_size);
    }

    for (int i = 0; i < guest_num; i++)
    {
        pthread_join(threads[i], NULL);
    }

	return 0;
}
