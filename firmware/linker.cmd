/*
 * TI Clang Linker Command File for SHIFT Project
 */

-uinterruptVectors
--stack_size=0x1800
--heap_size=0x0

MEMORY
{
    /*
     * Bootloader Region: 0x0000 - 0x6000 (Reserved)
     * Main Flash:        0x6000 - 0x3A000
     */
    BOOTLOADER  (RX) : origin = 0x00000000, length = 0x00006000
    FLASH_CODE  (RX) : origin = 0x00006000, length = 0x00010000   /* 64 KB  */

    SYSTEMCONF  (RW) : origin = 0x0001A000, length = 0x00000400   /*  1 KB  */
    METADATA    (RW) : origin = 0x0001A400, length = 0x00002000   /*  8 KB  */
    FILEDATA    (RW) : origin = 0x0001C400, length = 0x0001DC00   /* 119 KB */

    FAT         (RW) : origin = 0x0003A000, length = 0x00000400
    APP2        (RX) : origin = 0x0003A400, length = 0x00005C00

    /* SRAM: 32KB split into data (lower) and code (upper) */
    SRAM_DATA   (RW) : origin = 0x20200000, length = 0x00007000   /* 28 KB  */
    SRAM_CODE   (RX) : origin = 0x20207000, length = 0x00001000   /*  4 KB  */

    /* Config Regions */
    BCR_CONFIG  (R)  : origin = 0x41C00000, length = 0x000000FF
    BSL_CONFIG  (R)  : origin = 0x41C00100, length = 0x00000080
}

SECTIONS
{
    /* Interrupt Vectors at start of Main Flash */
    .intvecs:   > 0x00006000

    /*
     * Auto-copy function: copies .TI.ramfunc from FLASH to SRAM at startup
     * Runs from the upper SRAM partition (SRAM_CODE)
     */
    .TI.ramfunc   : load = FLASH_CODE, palign(8), run = SRAM_CODE, table(BINIT)
    {
        *dl_flashctl* (.text*)
        *dl_common* (.text*)
        *(.TI.ramfunc)
    }

    .text   : palign(8) {} > FLASH_CODE
    .const  : palign(8) {} > FLASH_CODE
    .cinit  : palign(8) {} > FLASH_CODE
    .pinit  : palign(8) {} > FLASH_CODE
    .rodata : palign(8) {} > FLASH_CODE
    .ARM.exidx    :  palign(8)  {} > FLASH_CODE
    .init_array   :  palign(8)  {} > FLASH_CODE
    .binit        : palign(8) {} > FLASH_CODE

    /* Data sections in lower SRAM partition */
    .args   :   > SRAM_DATA
    .data   :   > SRAM_DATA
    .bss    :   > SRAM_DATA
    .sysmem :   > SRAM_DATA
    .stack  :   > SRAM_DATA (HIGH)

    /* Flash data sections (persistent, no boot initialization) */
    .systemconf : (NOLOAD) palign(8) {} > SYSTEMCONF
    .metadata    : (NOLOAD) palign(8) {} > METADATA
    .filedata   : (NOLOAD) palign(8) {} > FILEDATA
    .fat         : (NOLOAD) palign(8) {} > FAT

    .BCRConfig : {} > BCR_CONFIG
    .BSLConfig : {} > BSL_CONFIG

}

/* Export boundary address for C code */
__sram_boundary = 0x20207000;
