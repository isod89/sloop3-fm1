/* SPDX-License-Identifier: GPL-3.0-only
 * The firmware in RAM: as a RAM app (hal/fm1/ramapp.ld: code, data, .bss and the stack from 0x01C30000), plus the
 * RAM below it that m-bootloader used before it started us (its stage-2 and loader slots, SPEC 11.8): the firmware
 * never returns to it, so it is ours. .lowbss (the sequencer's patterns) is cleared by whoever uses it.
 * .bootbss: m-bootloader's own RAM (SPEC 11.8: from 0x01C02000, its code, data and stack while it runs). It called
 * us on that stack, and the firmware never returns: once app_main runs on our stack, nothing there is used again.
 * Only buffers whose contents are written before they are read go there: the project's bytes for a save or a
 * load, the audio halves and mix buses (written each block), the screen's tile (drawn whole each time). */
ENTRY(_start)
MEMORY {
  boot (rw)  : ORIGIN = 0x01C02000, LENGTH = 0x9000
  low  (rw)  : ORIGIN = 0x01C0B000, LENGTH = 0x25000
  slot (rwx) : ORIGIN = 0x01C30000, LENGTH = 0x4FD80
  xip  (rx)  : ORIGIN = 0x02000400, LENGTH = 0x3B000   /* the flash window: the area from 0x400 (the projects at 0x40000) */
}
SECTIONS {
  /* the interface's code and its tables, run and read from the flash window (3.0B10): never while a save has the
   * window off (fw/main.c). The image: the stub at 0x120, this at 0x400, then the RAM image (fw/xip.ld) */
  .xtext : {
    */ui.o(.text* .rodata*) */gfx.o(.text* .rodata*) */fonts.o(.text* .rodata*) */presets.o(.text* .rodata*)
    */preset_data.o(.text* .rodata*) */engdesc.o(.text* .rodata*) */project.o(.text* .rodata*) */harmony.o(.text* .rodata*)
    */project.o(.text* .rodata*) */main.o(.rodata*) */sloopimport.o(.text* .rodata*) */sloop_kinds.o(.text* .rodata*) */usersnd.o(.text* .rodata*) */xbank.o(.text* .rodata*) */vis.o(.text* .rodata*) */slice_scan.o(.text* .rodata*) */demo_song.o(.text* .rodata*) */drumkits.o(.text* .rodata*)
    *(.xtext*)
    . = ALIGN(4);
  } > xip
  .entry : { KEEP(*(.entry)) } > slot
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > slot
  .data : { *(.data*) *(.sdata*) . = ALIGN(4); } > slot
  __image_end = .;
  __text_end = ADDR(.data);                                      /* (the code's end: BOOT_DIAG compares up to it) */
  .bss (NOLOAD) : { . = ALIGN(8); __bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(8); __bss_end = .; } > slot
  /* the stack: 7 KB (3.1: 1 KB to SLICE's tables); the deepest path measured is ~4 KB (the mix and the reverb, -fstack-usage), and the firmware
   * shows what it really used (HOME held) */
  /* 3.3.2: kept over a restart (the loader writes only the image; our start clears .bss only): fw/main.c's crumb */
  .noinit (NOLOAD) : { . = ALIGN(4); *(.noinit*) . = ALIGN(4); } > slot
  .lowbss (NOLOAD) : { . = ALIGN(8); *(.lowbss*) } > low
  /* (in the low RAM since 3.0B9: the slot's room goes to the code) */
  /* (3.4.1) its first 512 bytes the interrupts' stack (ssp: hal/fm1/ramapp_start.S; the tick's deepest is ~140 bytes,
   * tools/stackcheck.py), the rest the main stack (its deepest ~4.7 KB) */
  .stack (NOLOAD) : { . = ALIGN(8); __sstack_low = .; . += 0x200; __sstack_top = .; __stack_low = .; . += 0x1C00; __stack_top = .; } > low
  .bootbss (NOLOAD) : { . = ALIGN(8); *(.bootbss*) } > boot
  ASSERT(ADDR(.noinit) + SIZEOF(.noinit) <= 0x01C7FD80, "the firmware does not fit its slot")
}
