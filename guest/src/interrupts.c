#include "descriptors.h"
#include "interrupts.h"
#include "io.h"

static struct idt_entry idt[IDT_ENTRIES];

static volatile int mode = -1;
static uint8_t buffer[BUFFER_SIZE];
static volatile uint32_t buffer_size;
static volatile int write_ready;
static volatile int write_finished;
static volatile int write_succeeded;
static volatile int read_ready;

/*
	"general-regs-only" sprečava GCC da emituje SSE instrukcije, koje su zabranjene unutar
	__attribute__((interrupt)) handlera
*/
static void __attribute__((interrupt, target("general-regs-only")))
irq32_handler(struct interrupt_frame *frame)
{
    (void)frame;

    if (mode == -1)
    {
	    mode = inb(MODE_PORT);
        return;
    }

    if (mode == READING_MODE)
    {
        uint32_t size = inl(SIZE_PORT);
        uint32_t stored_size = size;

        if (stored_size > BUFFER_SIZE)
            stored_size = BUFFER_SIZE;

        for (uint32_t i = 0; i < size; ++i)
        {
            uint8_t value = inb(DATA_PORT);
            if (i < BUFFER_SIZE)
                buffer[i] = value;
        }

        buffer_size = stored_size;
        read_ready = 1;
        outl(SIZE_PORT, size);
    }
    else if (mode == WRITING_MODE && write_ready && !write_finished)
    {
        outl(SIZE_PORT, buffer_size);
        for (uint32_t i = 0; i < buffer_size; ++i)
            outb(DATA_PORT, buffer[i]);

        uint32_t written = inl(SIZE_PORT);
        write_succeeded = written == buffer_size;
        write_finished = 1;
        write_ready = 0;
    }
}

int interrupt_get_mode(void)
{
    return mode;
}

void interrupt_set_write_buffer(const uint8_t *data, uint32_t size)
{
    if (size > BUFFER_SIZE)
        size = BUFFER_SIZE;

    for (uint32_t i = 0; i < size; ++i)
        buffer[i] = data[i];

    buffer_size = size;
    write_finished = 0;
    write_succeeded = 0;
    write_ready = 1;
}

int interrupt_write_finished(void)
{
    return write_finished;
}

int interrupt_write_succeeded(void)
{
    return write_succeeded;
}

int interrupt_read_ready(void)
{
    return read_ready;
}

uint32_t interrupt_copy_read_buffer(uint8_t *destination, uint32_t capacity)
{
    uint32_t size = buffer_size;

    if (size > capacity)
        size = capacity;

    for (uint32_t i = 0; i < size; ++i)
        destination[i] = buffer[i];

    return size;
}

static void set_idt_gate(unsigned n, void (*handler)(struct interrupt_frame *))
{
	uint64_t addr = (uint64_t)(uintptr_t)handler;
	idt[n].offset_low  = addr & 0xFFFF;
	idt[n].selector    = 0x08;  /* 64-bit code segment */
	idt[n].ist         = 0;
	idt[n].type_attr   = 0x8E;  /* P=1, DPL=0, 64-bit interrupt gate */
	idt[n].offset_mid  = (addr >> 16) & 0xFFFF;
	idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
	idt[n].reserved    = 0;
}

void init_idt(void)
{
	struct dt_ptr p;

	set_idt_gate(32, irq32_handler);

	p.limit = sizeof(idt) - 1;
	p.base  = (uint64_t)(uintptr_t)idt;
	asm volatile("lidt %0" : : "m"(p) : "memory");
}
