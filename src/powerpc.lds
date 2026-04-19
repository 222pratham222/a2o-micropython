SECTIONS
{
    . = 0;
    _start = .;
    .head : {
        KEEP(*(.head))
    }

    /* Put text immediately after head — within ERAT range */
    .text   : { *(.text*)  *(.sfpr) }
    .rodata : { *(.rodata*) }
    .got2   : { *(.got2)  }
    .toc    : { *(.toc)   }
    .got    : { *(.got)   }

    . = ALIGN(8);
    .data : { *(.data*) *(.sdata*) }

    . = ALIGN(8);
    __bss_start = .;
    .bss : { *(.bss*) *(.sbss*) *(COMMON) }
    . = ALIGN(8);
    __bss_end = .;
}
