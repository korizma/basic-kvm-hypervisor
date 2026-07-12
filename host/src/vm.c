#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int  vm_init(struct vm *v, size_t mem_size)
{
	struct kvm_userspace_memory_region region;

	v->vm_fd = v->vcpu_fd = -1;
	v->mem = MAP_FAILED;
	v->run = MAP_FAILED;
	v->run_mmap_size = 0;
	v->mem_size = mem_size;

	int api = ioctl(v->kvm_fd, KVM_GET_API_VERSION, 0);

	if (api != KVM_API_VERSION) 
    {
		perror("KVM_API_VERSION");
		return -1;
	}

	v->vm_fd = ioctl(v->kvm_fd, KVM_CREATE_VM, 0);

	if (v->vm_fd < 0) 
    {
		perror("KVM_CREATE_VM");
		return -1;
	}

	v->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (v->mem == MAP_FAILED) 
    {
		perror("mmap mem");
		return -1;
	}

	region.slot = 0;
	region.flags = 0;
	region.guest_phys_addr = 0;
	region.memory_size = v->mem_size;
	region.userspace_addr = (uintptr_t)v->mem;

	if (ioctl(v->vm_fd, KVM_SET_USER_MEMORY_REGION, &region) < 0) 
    {
		perror("KVM_SET_USER_MEMORY_REGION");
		return -1;
	}

	v->vcpu_fd = ioctl(v->vm_fd, KVM_CREATE_VCPU, 0);

	if (v->vcpu_fd < 0) 
    {
		perror("KVM_CREATE_VCPU");
		return -1;
	}

	v->run_mmap_size = ioctl(v->kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);

	if (v->run_mmap_size <= 0) 
    {
		perror("KVM_GET_VCPU_MMAP_SIZE");
		return -1;
	}

	v->run = mmap(NULL, v->run_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, v->vcpu_fd, 0);

	if (v->run == MAP_FAILED) 
    {
		perror("mmap kvm_run");
		return -1;
	}

	return 0;
}

void vm_destroy(struct vm *v)
{
	if (v->run && v->run != MAP_FAILED) {
		munmap(v->run, (size_t)v->run_mmap_size);
		v->run = MAP_FAILED;
	}

	if (v->mem && v->mem != MAP_FAILED) {
		munmap(v->mem, v->mem_size);
		v->mem = MAP_FAILED;
	}

	if (v->vcpu_fd >= 0) {
		close(v->vcpu_fd);
		v->vcpu_fd = -1;
	}

	if (v->vm_fd >= 0) {
		close(v->vm_fd);
		v->vm_fd = -1;
	}

	if (v->kvm_fd >= 0) {
		close(v->kvm_fd);
		v->kvm_fd = -1;
	}
}

static void setup_segments_64(struct kvm_sregs *sregs)
{
	struct kvm_segment code = {
		.base    = 0,           // za segmente CS, DS, ES, SS u long modu base nije bitan, FS i GS vaze ali oni su extra segmenti
		.limit   = 0xffffffff,  // 
		.present = 1,           // bit for present/not present
		.type    = 0b1011,      // za s = 1, bit 0 - accesed, bit 1 readable/writable data, bit 2 conforming code, bit 3 executable 
		.dpl     = 0,           // descriptor privilege level, 0 - kernel segment, 3 - user segment
		.db      = 0,           // db = 0 for long mode, needs l = 1
		.s       = 1,           // s = 0 <=> system segment, s = 1 <=> code or data segment
		.l       = 1,           // l = 1, long mode, l = 0 32 or 16 mode
		.g       = 1,           // segment granuality, not really useful
	};
	struct kvm_segment data = code;
	data.type = 0b0011;
	data.l    = 0;

	sregs->cs = code;
	sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = data;
}

void setup_long_mode(struct vm *v, struct kvm_sregs *sregs, int page_size)
{
    // each table is 2^9 entries, 512 entires, 1 entry is 8B, so 512 * 8B = 4KB = 2^12 B
	uint64_t layer0_addr = 0x1000, layer1_addr = 0x2000, layer2_addr = 0x3000;
    uint64_t *layer0, *layer1, *layer2;

    layer0 = (void*)(v->mem + layer0_addr);
    layer1 = (void*)(v->mem + layer1_addr);
    layer2 = (void*)(v->mem + layer2_addr);


    if (page_size == PAGE_SIZE_2MB)
    {
        layer0[0] = PDE64_PRESENT | PDE64_RW | layer1_addr;
        layer1[0] = PDE64_PRESENT | PDE64_RW | layer2_addr;

        int page_number = v->mem_size / page_size;

        // setup pages begining from the guest program address
        for (int i = 0; i < page_number; i++)
        {
            layer2[i] = PDE64_PRESENT | PDE64_RW | PDE64_PS | (i * page_size);
        }
    }
    else if (page_size == PAGE_SIZE_4KB)
    {
        uint64_t layer3_addrs[4] = {0x4000, 0x5000, 0x6000, 0x7000};
        uint64_t *layer3[4];

        layer0[0] = PDE64_PRESENT | PDE64_RW | layer1_addr;
        layer1[0] = PDE64_PRESENT | PDE64_RW | layer2_addr;

        int page_number = v->mem_size / page_size;
        int l2_entries_number = page_number / 512;

        for (int i = 0; i < l2_entries_number; i++)
        {
            layer3[i] = (void*)(v->mem + layer3_addrs[i]);
            layer2[i] = PDE64_PRESENT | PDE64_RW | layer3_addrs[i];
        }

        for (int i = 0; i < l2_entries_number; i++)
        {
            for (int j = 0; j < 512; j++)
            {
                layer3[i][j] = PDE64_PRESENT | PDE64_RW | PDE64_PS | ((i*512 + j) * page_size);
            }
        }
    }

	sregs->cr3  = layer0_addr;
	sregs->cr4  = CR4_PAE;
	sregs->cr0  = CR0_PE | CR0_PG;
	sregs->efer = EFER_LME | EFER_LMA;

	setup_segments_64(sregs);
}

int load_guest_image(struct vm *v, const char *image_path, uint64_t load_addr)
{
	FILE *f = fopen(image_path, "rb");
	if (!f) {
		perror("Failed to open guest image");
		return -1;
	}

	if (fseek(f, 0, SEEK_END) < 0) {
		perror("Failed to seek to end of guest image");
		fclose(f);
		return -1;
	}

	long fsz = ftell(f);
	if (fsz < 0) {
		perror("Failed to get size of guest image");
		fclose(f);
		return -1;
	}
	rewind(f);

	if ((uint64_t)fsz > v->mem_size - load_addr) {
		printf("Guest image is too large for the VM memory\n");
		fclose(f);
		return -1;
	}

	if (fread((uint8_t *)v->mem + load_addr, 1, (size_t)fsz, f) != (size_t)fsz) {
		perror("Failed to read guest image");
		fclose(f);
		return -1;
	}
	fclose(f);

	return 0;
}

int inject_irq(struct vm *v, unsigned int vector)
{
	struct kvm_interrupt irq = { .irq = vector };

	if (ioctl(v->vcpu_fd, KVM_INTERRUPT, &irq) < 0) {
		perror("KVM_INTERRUPT");
		return -1;
	}
	return 0;
}
