#include "sched.h"

// quest: "two cooperative printers" (get help of gdb, AI)
// void cpu_switch_to(struct task_struct* prev, struct task_struct* next)
// save cpu regs (callee saved, sp/pc) to prev->cpu_context; 
// load next->cpu_context to the cpu regs
.globl cpu_switch_to
/* the context switch magic */
cpu_switch_to:
	mov	x10, #THREAD_CPU_CONTEXT     // sched.h
	add	x8, x0, x10
	// now `x8` will contain a pointer to the current `cpu_context`
	mov	x9, sp
	
	// Below: all callee-saved registers are stored in the order in which 
	// they are defined in the `cpu_context` structure.
	// Think: why not saving x11-x18? (compare to entry.S)
	stp	x19, x20, [x8], #16		
	stp	x21, x22, [x8], #16
	stp	x23, x24, [x8], #16
	stp	x25, x26, [x8], #16
	stp	x27, x28, [x8], #16
	stp	x29, x9, [x8], #16
	str	x30, [x8]			// save LR (x30) to cpu_context.pc, pointing to where this function is called from
	
	// calculate the address of the next task's `cpu_context`
add x8, x0, x0 /* TODO: replace this */
	
	// below: restore the CPU context of "switch_to" task to CPU regs
	/* TODO: your code here */
	ldr	x30, [x8]				// x30 == LR
	// restore the value of sp, which is already loaded from the cpu context
mov sp, x0 /* TODO: replace this */

	// The `ret` instruction will jump to the location pointed to by the link 
	// register (LR or `x30`). If we are switching to a task for the first time, 
	// this will be the beginning of the `ret_from_fork` function. In all other 
	// cases this will be the address previously saved in the `cpu_context.pc` 
	// by the `cpu_switch_to` function.
	ret							

