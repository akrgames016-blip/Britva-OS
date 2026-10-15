/*
 * Britva OS BIOS boot sector.
 * GNU as syntax; assembled in 32-bit mode with a 16-bit real-mode code section.
 * Reads 64 kernel sectors from a 1.44 MB floppy, enters 32-bit protected
 * mode, then transfers control to the freestanding C++ kernel.
 */

.section .text.boot
.code16
.global _start
_start:
    /* BIOS may enter at 0000:7c00 or 07c0:0000. Normalize CS. */
    cli
    ljmp $0x0000, $boot_start

boot_start:
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp

    movb %dl, boot_drive
    movb $2, disk_sector
    movb $0, disk_head
    movb $0, disk_cylinder
    movw $64, sectors_left

    /* Destination is 0x10000. A one-sector CHS read never crosses a track. */
    movw $0x1000, %ax
    movw %ax, %es
    xorw %bx, %bx

load_kernel_sector:
    /* Preserve the transfer cursor: some older BIOSes clobber general regs. */
    pushw %ds
    pushw %es
    pushw %bx
    pushw %cx
    pushw %dx
    movb $0x02, %ah
    movb $0x01, %al
    movb disk_cylinder, %ch
    movb disk_sector, %cl
    movb disk_head, %dh
    movb boot_drive, %dl
    sti
    int $0x13
    cli
    jc disk_error
    popw %dx
    popw %cx
    popw %bx
    popw %es
    popw %ds

    addw $512, %bx
    decw sectors_left
    jz kernel_loaded

    incb disk_sector
    cmpb $19, disk_sector       /* 1.44 MB floppy: 18 sectors per track. */
    jb load_kernel_sector
    movb $1, disk_sector
    incb disk_head
    cmpb $2, disk_head
    jb load_kernel_sector
    movb $0, disk_head
    incb disk_cylinder
    jmp load_kernel_sector

kernel_loaded:
    /* Preserve basic BIOS memory values at 0x500 for the kernel. */
    pushw %ds
    int $0x12
    popw %ds
    movw %ax, 0x0500            /* conventional memory in KiB */

    pushw %ds
    movb $0x88, %ah
    sti
    int $0x15
    cli
    popw %ds
    jc memory_query_failed
    movw %ax, 0x0502            /* extended memory above 1 MiB in KiB */
    jmp memory_query_done
memory_query_failed:
    movw $0, 0x0502
memory_query_done:
    /* Enable A20 before entering protected mode. */
    inb $0x92, %al
    orb $0x02, %al
    andb $0xfe, %al
    outb %al, $0x92

    cli
    lgdt gdt_descriptor
    movl %cr0, %eax
    orl $0x00000001, %eax
    movl %eax, %cr0
    ljmp $0x08, $protected_mode

disk_error:
    cli
1:
    hlt
    jmp 1b

.code32
protected_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    /* The kernel entry installs its own stack and clears .bss. */
    movl $0x10000, %eax
    jmp *%eax

.align 8
gdt:
    .quad 0x0000000000000000
    .quad 0x00cf9a000000ffff  /* 0x08: ring-0, 32-bit code */
    .quad 0x00cf92000000ffff  /* 0x10: ring-0, 32-bit data */
gdt_end:
gdt_descriptor:
    .word gdt_end - gdt - 1
    .long gdt

boot_drive:
    .byte 0
disk_sector:
    .byte 0
disk_head:
    .byte 0
disk_cylinder:
    .byte 0
sectors_left:
    .word 0

/* BIOS requires the boot signature in bytes 510-511. */
.org 510, 0
.word 0xaa55

.section .note.GNU-stack,"",@progbits
