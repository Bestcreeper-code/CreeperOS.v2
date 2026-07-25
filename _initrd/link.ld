ENTRY(_start)

SECTIONS
{
    . = 0x400000; /* match DEFAULT_BIN_EXEC_ENTRY or wherever you map segments */

    .text : {
        *(.text*)
    }

    . = ALIGN(0x1000);
    .rodata : {
        *(.rodata*)
    }

    . = ALIGN(0x1000);
    .data : {
        *(.data*)
    }

    . = ALIGN(0x1000);
    .bss : {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}