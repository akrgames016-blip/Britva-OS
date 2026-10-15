OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* read + execute */
    data PT_LOAD FLAGS(6); /* read + write */
}

SECTIONS
{
    . = 0x00010000;
    __kernel_start = .;

    .text ALIGN(16) :
    {
        KEEP(*(.text.boot))
        *(.text .text.*)
    } :text

    .rodata ALIGN(16) :
    {
        *(.rodata .rodata.*)
    } :text

    .data ALIGN(16) :
    {
        *(.data .data.*)
    } :data

    .bss ALIGN(16) (NOLOAD) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(16);
        __bss_end = .;
    } :data

    __kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.got*)
        *(.plt*)
    }
}
