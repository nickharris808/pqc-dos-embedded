/* embedded.ld — minimal bare-metal map for QEMU lm3s6965evb (Cortex-M3).
 * FLASH 256 KiB @ 0x0, SRAM 64 KiB @ 0x20000000. This RAM LENGTH is the hard
 * "device SRAM" ceiling: the freestanding bump allocator's arena lives in .bss
 * here, so a naive flood far larger than 64 KiB cannot be held. */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
  RAM   (rwx): ORIGIN = 0x20000000, LENGTH = 64K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text       : { *(.text*) *(.rodata*) } > FLASH
  . = ALIGN(4);
  _sidata = LOADADDR(.data);
  .data : { _sdata = .; *(.data*) ; _edata = .; } > RAM AT > FLASH
  .bss  : { _sbss = .; *(.bss*) *(COMMON) ; _ebss = .; } > RAM
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
