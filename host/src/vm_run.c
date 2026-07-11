#include "vm_run.h"
#include "stdio.h"

pthread_t* create_vm_thread(int kvm_fd, const char* guest_file, uint64_t memory_size, uint64_t page_size)
{
    struct vm* v = (struct vm*)malloc(sizeof(struct vm)); 

    v->kvm_fd = kvm_fd;
    v->mem_size = memory_size;
    v->page_size = page_size;

	struct kvm_sregs sregs;
	struct kvm_regs regs;

    if (vm_init(v, memory_size)) {
		printf("Failed to init the VM\n");
		return 1;
	}

	if (ioctl(v->vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		vm_destroy(&v);
		return 1;
	}

	setup_long_mode(v, &sregs, page_size);

	if (ioctl(v->vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		vm_destroy(&v);
		return 1;
	}

    uint64_t guest_start_addr;
    if (page_size == PAGE_SIZE_2MB)
        guest_start_addr = GUEST_START_ADDR_PS_2MB;
    else
        guest_start_addr = GUEST_START_ADDR_PS_4KB;

	if (load_guest_image(v, guest_file, guest_start_addr) < 0) {
		printf("Failed to load guest image\n");
		vm_destroy(&v);
		return 1;
	}

	memset(&regs, 0, sizeof(regs));
	regs.rflags = 0x2;
	regs.rip    = guest_start_addr;
	regs.rsp    = guest_start_addr + memory_size - 1;

	if (ioctl(v->vcpu_fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		vm_destroy(&v);
		return 1;
	}

    pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t));

    int result = pthread_create(thread, NULL, vm_main_thread, v);

    if (result != 0) {
        printf("pthread_create failed: %d\n", result);
        return;
    }

    return thread;
}

void* vm_main_thread(struct vm* v)
{
    int stop = 0;
    int ret;

    while (stop == 0) {
		ret = ioctl(v->vcpu_fd, KVM_RUN, 0);
		if (ret == -1) {
			printf("KVM_RUN failed\n");
			vm_destroy(&v);
			return 1;
		}

		switch (v->run->exit_reason) {
		case KVM_EXIT_IO:
			if (v->run->io.direction == KVM_EXIT_IO_OUT && v->run->io.port == 0xE9) {
				char *p = (char *)v->run;
				printf("%c", *(p + v->run->io.data_offset));
			}
			continue;
		// case KVM_EXIT_IRQ_WINDOW_OPEN:
		// 	if (irq_pending > 0) {
		// 		if (inject_irq(&v, IRQ_NUM) < 0) {
		// 			vm_destroy(&v);
		// 			return 1;
		// 		}
		// 		irq_pending--;
		// 	} else {
		// 		v->run->request_interrupt_window = 0;
		// 	}
		// 	continue;
		case KVM_EXIT_HLT:
			printf("KVM_EXIT_HLT\n");
			stop = 1;
			break;
		case KVM_EXIT_SHUTDOWN:
			printf("Shutdown\n");
			stop = 1;
			break;
		default:
			printf("Default - exit reason: %d\n", v->run->exit_reason);
			break;
		}
	}

    vm_destroy(v);
    free(v);
}