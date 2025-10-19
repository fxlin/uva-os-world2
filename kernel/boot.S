/*
 * Kernel boot code
 * 
 * No MMU/pgtable, single core, has bootstack (for multitasking).
 * Shall be used with newer rpi3 firmware that builds in
 * armstub, which boots kernel at 0x80000 and EL2.
 */

#include "param.h"
#include "sysregs.h"
#include "plat.h"

.section ".text.boot"

.globl _start
_start:		
	// MMU off, until we set pgtables. cf: sysregs.h
	ldr	x0, =SCTLR_VALUE_MMU_DISABLED  
	msr	sctlr_el1, x0
	
	/* -------- Exception level switch -------------- */
	// Check the current exception level: EL3 or EL2?	
	mrs x0, CurrentEL
  	lsr x0, x0, #2
	cmp x0, #3
	beq el3

	# Current EL: EL2 
	# set EL1 to be running in AArch64
	mrs	x0, hcr_el2
	orr	x0, x0, #HCR_RW  
	msr	hcr_el2, x0

	# prepare to switch to EL1
	mov x0, #SPSR_VALUE
	msr	spsr_el2, x0

	adr	x0, el1_entry
	msr	elr_el2, x0
	eret	// switch to EL1

el3: 		// Current EL: EL3
	// 	With the rpi3 firmware (armstub) or qemu, kernel always starts in EL2; 
	//  We leave EL3 code here for completeness
  	ldr x0, =HCR_VALUE
  	msr hcr_el2, x0

	ldr	x0, =SCR_VALUE
	msr	scr_el3, x0

	# prepare to switch to EL1
	ldr	x0, =SPSR_VALUE
	msr	spsr_el3, x0

	adr	x0, el1_entry		
	msr	elr_el3, x0	
	eret	// switch to EL1				

	/* ------- Start of EL1 execution ------- */
el1_entry:	
	// Clean up bss region. 
	// bss_begin/end are linking addr (kernel virt). convert them to phys.
	// they are (at least) 8 bytes aligned in link script
	ldr	x0, =bss_begin
	ldr	x1, =bss_end
	sub	x1, x1, x0
	bl 	memzero_aligned
	
setup_sp: 	
	// quest: boot. Set sp to be at the end (or top) of the bootstack (cpu0)
	/* TODO: your code here */
	ldr x1, =boot_stacks	// sched.c		
	/* TODO: your code here */
	// NB: we aren't use sp yet -- until we call a C function for the 1st time

	// install irq vectors
	ldr x0, =vectors	// load VBAR_EL1 vector table addr
	msr	vbar_el1, x0	

	// load the addr of kernel_main
	bl kernel_main  	// kernel.c	