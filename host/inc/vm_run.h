#ifndef VM_RUN_H
#define VM_RUN_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include "vm.h"

pthread_t* create_vm_thread(int thread_id, int kvm_fd, const char* guest_file, uint64_t memory_size, uint64_t page_size);

int vm_main_thread(struct vm* vm);

void print_vm_debug(struct vm *v);

#endif /* VM_RUN_H */