/* 32-bit exception and hardware IRQ entry points. */
.section .text
.code32
.extern exception_handler
.extern irq0_handler
.extern irq1_handler
.extern irq4_handler

.macro ISR_NO_ERROR vector
.global isr\vector
.type isr\vector, @function
isr\vector:
    pushl $0
    pushl $\vector
    jmp exception_common
.endm

.macro ISR_WITH_ERROR vector
.global isr\vector
.type isr\vector, @function
isr\vector:
    pushl $\vector
    jmp exception_common
.endm

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_WITH_ERROR 8
ISR_NO_ERROR 9
ISR_WITH_ERROR 10
ISR_WITH_ERROR 11
ISR_WITH_ERROR 12
ISR_WITH_ERROR 13
ISR_WITH_ERROR 14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_WITH_ERROR 17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_WITH_ERROR 21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_WITH_ERROR 29
ISR_WITH_ERROR 30
ISR_NO_ERROR 31

exception_common:
    cld
    pushal
    pushl 32(%esp)               /* vector number, before error code */
    call exception_handler
    addl $4, %esp
    popal
    addl $8, %esp                /* vector + error code */
    iret

.global irq0_stub
.type irq0_stub, @function
irq0_stub:
    cld
    pushal
    call irq0_handler
    popal
    iret

.global irq1_stub
.type irq1_stub, @function
irq1_stub:
    cld
    pushal
    call irq1_handler
    popal
    iret

.global irq4_stub
.type irq4_stub, @function
irq4_stub:
    cld
    pushal
    call irq4_handler
    popal
    iret

.section .note.GNU-stack,"",@progbits
