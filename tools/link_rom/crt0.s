@ The cartridge header and reset: IRQ and system stacks in IWRAM, then main in Thumb.
    .section .header, "ax"
    .arm
    .global _start
_start:
    b reset
    .space 156
    .ascii "OPENSU LINK "
    .ascii "OSLT"
    .ascii "00"
    .byte 0x96
    .space 10
    .byte 0
    .space 2
reset:
    mov r0, #0x12
    msr cpsr_c, r0
    ldr sp, =0x03007fa0
    mov r0, #0x1f
    msr cpsr_c, r0
    ldr sp, =0x03007f00
    ldr r0, =main
    bx r0
    .ltorg
