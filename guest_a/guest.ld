OUTPUT_FORMAT(binary)
SECTIONS
{
        .start 0x8000: { *(.start) }
        .text : { *(.text*) }
        .rodata : { *(.rodata) }
        .data : { *(.data) }
        .bss  : { *(.bss*) }
}
