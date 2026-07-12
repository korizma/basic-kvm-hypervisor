#include "vm_run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

void *vm_thread_wrapper(void *arg)
{
    struct vm *v = arg;

    int result = vm_main_thread(v);

    return (void *)(intptr_t)result;
}

pthread_t* create_vm_thread(int thread_id, int kvm_fd, const char* guest_file, uint64_t memory_size, uint64_t page_size)
{
    struct vm* v = (struct vm*)malloc(sizeof(struct vm)); 
	memset(v, 0, sizeof(*v));

    v->kvm_fd = kvm_fd;
    v->mem_size = memory_size;
    v->page_size = page_size;
    v->thread_id = thread_id;

	struct kvm_sregs sregs;
	struct kvm_regs regs;

    if (vm_init(v, memory_size)) {
		printf("Failed to init the VM\n");
		return 0;
	}

	if (ioctl(v->vcpu_fd, KVM_GET_SREGS, &sregs) < 0) {
		// perror("KVM_GET_SREGS");
        fprintf(stderr, "Thread %d: KVM_GET_REGS: %s\n", thread_id, strerror(errno));
		vm_destroy(v);
		return 0;
	}

	setup_long_mode(v, &sregs, page_size);

	if (ioctl(v->vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
		// perror("KVM_SET_SREGS");
        fprintf(stderr, "Thread %d: KVM_SET_SREGS: %s\n", thread_id, strerror(errno));
		vm_destroy(v);
		return 0;
	}

    uint64_t guest_start_addr = GUEST_START_ADDR;
        
	if (load_guest_image(v, guest_file, guest_start_addr) < 0) 
    {
		printf("Thread %d: Failed to load guest image\n", thread_id);
		vm_destroy(v);
		return 0;
	}

	memset(&regs, 0, sizeof(regs));
	regs.rflags = 0x2;
	regs.rip    = guest_start_addr;
	regs.rsp    = (memory_size - 1) & (uint64_t)(~(0xFF));

	if (ioctl(v->vcpu_fd, KVM_SET_REGS, &regs) < 0) {
		// perror("KVM_SET_REGS");
        fprintf(stderr, "Thread %d: KVM_SET_SREGS: %s\n", thread_id, strerror(errno));
		vm_destroy(v);
		return 0;
	}

    pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t));

    int result = pthread_create(thread, NULL, vm_thread_wrapper, v);

    if (result != 0) {
        fprintf(stderr, "Thread %d: pthread_create failed: %s\n", thread_id, strerror(errno));
        return 0;
    }

    return thread;
}

int vm_main_thread(struct vm* v)
{
    int stop = 0;
    int ret;

    while (stop == 0) {
		ret = ioctl(v->vcpu_fd, KVM_RUN, 0);
		if (ret == -1) {
			printf("Thread %d: KVM_RUN failed\n", v->thread_id);
			vm_destroy(v);
			return 1;
		}

		switch (v->run->exit_reason) 
        {
            case KVM_EXIT_IO:
                if (v->run->io.direction == KVM_EXIT_IO_OUT && v->run->io.port == 0xE9) 
                {
                    char *p = (char *)v->run;
                    // printf("Thread %d: %c\n", v->thread_id, *(p + v->run->io.data_offset));
                    printf("%c", *(p + v->run->io.data_offset));
                }

                else if (v->run->io.direction == KVM_EXIT_IO_IN && v->run->io.port == 0xE9) 
                {
                    char c_sent = 'W';
                    char *input = (char*)v->run + v->run->io.data_offset;
                    *input = c_sent;
                    // printf("Thread %d: Sent char: %c\n", v->thread_id, c_sent);
                    printf("\nSent char: %c\n", c_sent);
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
			printf("Thread %d: KVM_EXIT_HLT\n", v->thread_id);
			stop = 1;
			break;
		case KVM_EXIT_SHUTDOWN:
			printf("Thread %d: Shutdown\n", v->thread_id);
			stop = 1;
			break;
        case KVM_EXIT_INTERNAL_ERROR:
            print_vm_debug(v);
            break;
		default:
			printf("Thread %d: Default - exit reason: %d\n", v->thread_id, v->run->exit_reason);
			break;
		}
	}

    vm_destroy(v);
    free(v);
    return 0;
}

void print_vm_debug(struct vm *v)
{
    fprintf(stderr, "KVM internal error: suberror=%u, ndata=%u\n", v->run->internal.suberror, v->run->internal.ndata);

    for (uint32_t i = 0; i < v->run->internal.ndata && i < 16; i++) 
    {
        fprintf(stderr, "data[%u] = 0x%016llx\n", i, (unsigned long long)v->run->internal.data[i]);
    }
        
    struct kvm_regs regs;
    struct kvm_sregs sregs;

    ioctl(v->vcpu_fd, KVM_GET_REGS, &regs);
    ioctl(v->vcpu_fd, KVM_GET_SREGS, &sregs);

    fprintf(stderr, "RIP   = 0x%llx\n", (unsigned long long)regs.rip);
    fprintf(stderr, "RSP   = 0x%llx\n", (unsigned long long)regs.rsp);
    fprintf(stderr, "RFLAGS= 0x%llx\n", (unsigned long long)regs.rflags);
    fprintf(stderr, "CR0   = 0x%llx\n", (unsigned long long)sregs.cr0);
    fprintf(stderr, "CR3   = 0x%llx\n", (unsigned long long)sregs.cr3);
    fprintf(stderr, "CR4   = 0x%llx\n", (unsigned long long)sregs.cr4);
    fprintf(stderr, "EFER  = 0x%llx\n", (unsigned long long)sregs.efer);
    fprintf(stderr, "CS.L  = %u\n", sregs.cs.l);
    fprintf(stderr, "CS.DB = %u\n", sregs.cs.db);
}