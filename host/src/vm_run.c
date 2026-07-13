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

pthread_t* create_vm_thread(int thread_id, int kvm_fd, const char* guest_file, uint64_t memory_size, uint64_t page_size, struct file_base* file_base, struct buffer* buffer)
{
    struct vm* v = (struct vm*)malloc(sizeof(struct vm)); 
	memset(v, 0, sizeof(*v));

    v->kvm_fd = kvm_fd;
    v->mem_size = memory_size;
    v->page_size = page_size;
    v->thread_id = thread_id;

    create_file_base_using_initial_base(&v->file_base, file_base);

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

	if (ioctl(v->vcpu_fd, KVM_SET_REGS, &regs) < 0) 
    {
		// perror("KVM_SET_REGS");
        fprintf(stderr, "Thread %d: KVM_SET_SREGS: %s\n", thread_id, strerror(errno));
		vm_destroy(v);
		return 0;
	}

    v->buffer = buffer;

    pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t));

    int result = pthread_create(thread, NULL, vm_thread_wrapper, v);

    if (result != 0) 
    {
        fprintf(stderr, "Thread %d: pthread_create failed: %s\n", thread_id, strerror(errno));
        return 0;
    }

    return thread;
}

int vm_main_thread(struct vm* v)
{
    int stop = 0;
    int ret;

    struct file_operation op;
    op.buffer_can_be_freed = false;
    clear_file_operation(&op);

    bool is_writer = v->thread_id == 0;
    bool is_reader = !is_writer;
    bool mode_sent = false;
    bool has_written_current_buffer = false;
    bool has_read_current_buffer = false;

    uint32_t curr_pos = 0;


    bool irq_pending = true;
    v->run->request_interrupt_window = 1;

    while (stop == 0) {

		ret = ioctl(v->vcpu_fd, KVM_RUN, 0);
		if (ret == -1) {
			printf("Thread %d: KVM_RUN failed\n", v->thread_id);
			vm_destroy(v);
			return 1;
		}

        if ((v->buffer->size == 0 && curr_pos == 0 && is_writer && !has_written_current_buffer) ||
            (v->buffer->size > 0 && curr_pos == 0 && !v->buffer->writing && !v->buffer->reading && is_reader && !has_read_current_buffer))
        {
            if (!irq_pending)
            {
                v->run->request_interrupt_window = 1;
            }
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
                    char c_sent = 'a' + v->thread_id;
                    char *input = (char*)v->run + v->run->io.data_offset;
                    *input = c_sent;
                    // printf("Thread %d: Sent char: %c\n", v->thread_id, c_sent);
                    // printf("\nSent char: %c\n", c_sent);
                }
                else if (v->run->io.direction == KVM_EXIT_IO_OUT && v->run->io.port == FILE_PORT) 
                {
                    uint32_t data_recieved = *(uint32_t *)((char *)v->run + v->run->io.data_offset);
                    advance_state_file_operation(&op, data_recieved);
                }
                else if (v->run->io.direction == KVM_EXIT_IO_IN && v->run->io.port == FILE_PORT) 
                {
                    int ret_val = execute_file_operation(v, &op);
                    *(int32_t *)((char *)v->run + v->run->io.data_offset) = ret_val;
                    clear_file_operation(&op);
                }
                else if (v->run->io.direction == KVM_EXIT_IO_OUT && v->run->io.port == SIZE_PORT) 
                {
                    pthread_mutex_lock(&v->buffer->mutex);
                    uint32_t data_recieved = *(uint32_t *)((char *)v->run + v->run->io.data_offset);
                    
                    // pisac salje koliko podataka pise
                    if (is_writer && v->buffer->size == 0 && curr_pos == 0)
                    {
                        irq_pending = false;
                        v->buffer->size = data_recieved;
                        v->buffer->writing = true;
                    }

                    bool invalid_read_num = false;
                    // reader sends number of read data
                    if (is_reader && v->buffer->reading)
                    {
                        if (data_recieved != v->buffer->size)
                            invalid_read_num = true;
                        
                        v->buffer->reading = false;
                        curr_pos = 0;
                        has_read_current_buffer = true;
                        v->buffer->readers_finished++;
                        if (v->buffer->readers_finished == v->buffer->reader_num)
                        {
                            v->buffer->readers_finished = 0;
                            v->buffer->size = 0;
                        }
                    }

                    if (invalid_read_num)
                    {
                        printf("Thread %d: Read wrong amount of bytes from buffer, exiting...", v->thread_id);
                        v->buffer->reader_num--;
                        pthread_mutex_unlock(&v->buffer->mutex);
                        return 1;
                    }
                    pthread_mutex_unlock(&v->buffer->mutex);
                }
                else if (v->run->io.direction == KVM_EXIT_IO_IN && v->run->io.port == SIZE_PORT) 
                {
                    pthread_mutex_lock(&v->buffer->mutex);
                    int32_t* loc_to_send = (int32_t *)((char *)v->run + v->run->io.data_offset);

                    // writing finished now sending written num of data
                    if (is_writer && v->buffer->writing && v->buffer->size == curr_pos)
                    {
                        *loc_to_send = curr_pos;
                        curr_pos = 0;
                        v->buffer->writing = false;
                        has_written_current_buffer = true;
                    }
                    
                    // reading starting, sending number of data in buffer
                    if (is_reader && !v->buffer->writing && !v->buffer->reading && curr_pos == 0)
                    {
                        irq_pending = false;
                        v->buffer->reading = true;
                        *loc_to_send = v->buffer->size;
                    }

                    pthread_mutex_unlock(&v->buffer->mutex);
                }
                else if (v->run->io.direction == KVM_EXIT_IO_OUT && v->run->io.port == DATA_PORT) 
                {
                    pthread_mutex_lock(&v->buffer->mutex);
                    uint8_t data_recieved = *(uint8_t *)((char *)v->run + v->run->io.data_offset);

                    // pisac je poslao velicinu sada salje podatke
                    if (is_writer && v->buffer->writing && v->buffer->size > curr_pos)
                    {
                        v->buffer->buffer[curr_pos++] = data_recieved;
                    }
                    pthread_mutex_unlock(&v->buffer->mutex);
                }
                else if (v->run->io.direction == KVM_EXIT_IO_IN && v->run->io.port == DATA_PORT) 
                {
                    uint8_t* loc_to_send = (uint8_t *)v->run + v->run->io.data_offset;

                    if (!mode_sent)
                    {
                        mode_sent = true;
                        irq_pending = false;
                        if (is_reader)
                        {
                            *loc_to_send = 0;
                        }
                        if (is_writer)
                        {
                            *loc_to_send = 1;
                        }
                        break;
                    }

                    pthread_mutex_lock(&v->buffer->mutex);
                    
                    // reading from buffer
                    if (is_reader && v->buffer->reading && curr_pos < v->buffer->size)
                    {
                        *loc_to_send = v->buffer->buffer[curr_pos++];
                    }
                    pthread_mutex_unlock(&v->buffer->mutex);
                }
			continue;
		case KVM_EXIT_IRQ_WINDOW_OPEN:
            if (v->run->ready_for_interrupt_injection) 
            {

                struct kvm_interrupt irq = {
                    .irq = 32
                };

                if (ioctl(v->vcpu_fd, KVM_INTERRUPT, &irq) < 0) {
                    perror("KVM_INTERRUPT");
                    break;
                }

                irq_pending = true;
                v->run->request_interrupt_window = 0;
            }
            break;

		case KVM_EXIT_HLT:
            if (!irq_pending)
            {
                bool operation_finished =
                    (is_writer && has_written_current_buffer) ||
                    (is_reader && has_read_current_buffer);

                if (operation_finished)
                {
                    printf("Thread %d: KVM_EXIT_HLT\n", v->thread_id);
			        stop = 1;
                }
            }
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

    if (is_reader && !has_read_current_buffer)
    {
        pthread_mutex_lock(&v->buffer->mutex);
        v->buffer->reader_num--;
        pthread_mutex_unlock(&v->buffer->mutex);
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
