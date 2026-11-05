/* Everything in the cartridge ROM; the program keeps no globals. */
ENTRY(_start)
SECTIONS {
    . = 0x08000000;
    .header : { KEEP(*(.header)) }
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    /DISCARD/ : { *(.ARM.*) *(.comment) }
    ASSERT(SIZEOF(.data) == 0 && SIZEOF(.bss) == 0, "the link test keeps no globals")
}
