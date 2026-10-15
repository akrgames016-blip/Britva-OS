OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0x7c00;
    .boot :
    {
        KEEP(*(.text.boot))
    }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
