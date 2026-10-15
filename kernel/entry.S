/* 32-bit protected-mode entry point for the C++ kernel. */
.section .text.boot
.code32
.global _start
.type _start, @function
.extern kernel_main
.extern __bss_start
.extern __bss_end
.extern kernel_stack_top

_start:
    cli
    cld
    movl $kernel_stack_top, %esp

    /* The BIOS loader only loads file-backed bytes; zero the kernel's .bss. */
    xorl %eax, %eax
    movl $__bss_start, %edi
    movl $__bss_end, %ecx
    subl %edi, %ecx
    rep stosb

    call kernel_main

1:
    cli
    hlt
    jmp 1b
.size _start, . - _start

.section .bss
.align 16
.skip 16384
.global kernel_stack_top
kernel_stack_top:

.section .note.GNU-stack,"",@progbits
