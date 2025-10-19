/* linker script for rpi3 hardware. 
	shall be used with newer rpi3 firmware that builds in
	armstub, which boots kernel at 0x80000 and EL2
*/

SECTIONS
{
	. = 0x80000;  
	
	.text.boot : { *(.text.boot) }
	.text :  { *(.text) }
	.rodata : { *(.rodata) }
	.data : { *(.data) }
	
	. = ALIGN(0x8);	 /* bss begin/end must be aligned: cleared with memzero_aligned*/	
	bss_begin = .;
	.bss : { *(.bss*) } 
	bss_end = .;
	
	kernel_end = .; 
}