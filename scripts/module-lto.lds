/* SPDX-License-Identifier: GPL-2.0 */
/* Merge LLVM-generated per-function sections and matching relocations. */
SECTIONS {
	.text : { *(.text .text.[0-9a-zA-Z_]*) }
	.data : { *(.data .data.[0-9a-zA-Z_]*) }
	.bss : { *(.bss .bss.[0-9a-zA-Z_]*) }
	.rodata : { *(.rodata .rodata.[0-9a-zA-Z_]*) }
	.rela.text : { *(.rela.text .rela.text.[0-9a-zA-Z_]*) }
	.rela.data : { *(.rela.data .rela.data.[0-9a-zA-Z_]*) }
	.rela.rodata : { *(.rela.rodata .rela.rodata.[0-9a-zA-Z_]*) }
}
