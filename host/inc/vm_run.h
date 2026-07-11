#ifndef VM_RUN_H
#define VM_RUN_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include "vm.h"

pthread_t* create_vm_thread(int kvm_fd, const char* guest_file, uint64_t memory_size, uint64_t page_size);

void* vm_main_thread(struct vm* vm);

#endif /* VM_RUN_H */