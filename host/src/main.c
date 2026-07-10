#include "vm.h"
#include "arg_reader.h"

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/kvm.h>

// is log2(MEM_SIZE)
#define PHYS_MEM_SIZE_2MB 21
#define PHYS_MEM_SIZE_4MB 22
#define PHYS_MEM_SIZE_8MB 23

// is log2(PAGE_SIZE)
#define PAGE_SIZE_4KB 12
#define PAGE_SIZE_2MB 21

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

    char* phys_mem_string = get_arg_single_2_options(argv, argc, "-m", "--memory");
    char* page_size_string = get_arg_single_2_options(argv, argc, "-p", "--page");

    char** guest_strings = get_arg_array_2_options(argv, argc, "-g", "--guest");

    if (phys_mem_string == 0)
    {
		printf("The program requests a memory size: %s --memory <2 or 4 or 8>\n", argv[0]);
        return 1;
    }

    if (page_size_string == 0)
    {
		printf("The program requests a page size: %s --page <2 or 4>\n", argv[0]);
        return 1;
    }

    if (guest_strings == 0)
    {
		printf("The program requests guest programs: %s --guest <list-of-filename>\n", argv[0]);
        return 1;
    }

    char* filename = guest_strings[0];

    phys_mem_size = phys_mem_string[0] - '0';
    page_size = page_size_string[0] - '0';

	if (vm_init(&v, phys_mem_size)) {
		printf("Failed to init the VM\n");
		return 1;
	}

	if (ioctl(v.vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		vm_destroy(&v);
		return 1;
	}

	setup_long_mode(&v, &sregs);

	if (ioctl(v.vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		vm_destroy(&v);
		return 1;
	}

	if (load_guest_image(&v, argv[1], GUEST_START_ADDR) < 0) {
		printf("Failed to load guest image\n");
		vm_destroy(&v);
		return 1;
	}

	memset(&regs, 0, sizeof(regs));
	regs.rflags = 0x2;
	regs.rip    = 0;
	regs.rsp    = 2 << 20;

	if (ioctl(v.vcpu_fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		vm_destroy(&v);
		return 1;
	}

	v.run->request_interrupt_window = (irq_pending > 0);

	while (stop == 0) {
		ret = ioctl(v.vcpu_fd, KVM_RUN, 0);
		if (ret == -1) {
			printf("KVM_RUN failed\n");
			vm_destroy(&v);
			return 1;
		}

		switch (v.run->exit_reason) {
		case KVM_EXIT_IO:
			if (v.run->io.direction == KVM_EXIT_IO_OUT && v.run->io.port == 0xE9) {
				char *p = (char *)v.run;
				printf("%c", *(p + v.run->io.data_offset));
			}
			continue;
		case KVM_EXIT_IRQ_WINDOW_OPEN:
			if (irq_pending > 0) {
				if (inject_irq(&v, IRQ_NUM) < 0) {
					vm_destroy(&v);
					return 1;
				}
				irq_pending--;
			} else {
				v.run->request_interrupt_window = 0;
			}
			continue;
		case KVM_EXIT_HLT:
			printf("KVM_EXIT_HLT\n");
			stop = 1;
			break;
		case KVM_EXIT_SHUTDOWN:
			printf("Shutdown\n");
			stop = 1;
			break;
		default:
			printf("Default - exit reason: %d\n", v.run->exit_reason);
			break;
		}
	}

	vm_destroy(&v);
	return 0;
}
