#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

#define BUFFER_SIZE 100

#define SIZE_PORT 0x520
#define DATA_PORT 0x510
#define MODE_PORT 0x510

#define READING_MODE 0
#define WRITING_MODE 1

void init_idt(void);
int interrupt_get_mode(void);
void interrupt_set_write_buffer(const uint8_t *data, uint32_t size);
int interrupt_write_finished(void);
int interrupt_write_succeeded(void);
int interrupt_read_ready(void);
uint32_t interrupt_copy_read_buffer(uint8_t *destination, uint32_t capacity);

#endif /* INTERRUPTS_H */
