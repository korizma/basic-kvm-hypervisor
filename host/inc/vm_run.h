#ifndef VM_RUN_H
#define VM_RUN_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include "vm.h"
#include "files.h"
#include "buffer.h"

#define FILE_PORT 0x0278
#define DATA_PORT 0x510
#define SIZE_PORT 0x520

pthread_t* create_vm_thread(int thread_id, int kvm_fd, const char* guest_file, uint64_t memory_size, uint64_t page_size, struct file_base *base, struct buffer* buffer);

int vm_main_thread(struct vm* vm);

void print_vm_debug(struct vm *v);

#endif 