#include "sysregs.h"
#include "entry.h"
#include "plat.h"
#include "param.h"


	/* A default handler just to print out meaningful message in case our kernel
	 triggers some unwanted exceptions due to our programming mistakes */
.macro handle_invalid_entry type
	kernel_entry
	// below: arguments to handle_invalid_entry are passed in 3 registers: x0, x1, and x2.
	// x0: the exception type. It can take one of the values defined in entry.h. It tells us exactly which exception handler has been executed
	mov	x0, #\type   
	// x1: what causes the exception. Value from `esr_el1` (Exception Syndrome Register). EL1 implies "when the exception is handled at EL1".
	mrs	x1, esr_el1
	// x2: the address of the instruction being executed when the exception happens. EL1 implies "when taking an exception to EL1, (this reg) holds the address to return to" 
	mrs	x2, elr_el1
	// x3: data faulty address 
	mrs x3, far_el1
	bl	show_invalid_entry_message	// irq.c
	msr	daifset, #0b0010 		// disable irq
	b	err_hang
	.endm
	
/* -------------------------------------------------------------------------- */	
.macro	ventry	label
	.align	7	// We need `.align 7` because all exception vectors should be spaced at `0x80` bytes (2<<7) one from another
	b	\label
	.endm

/* -------------------------------------------------------------------------- */
// quest: "two preemptive printers". compare to switch.S. why save x0-x18 here but not in switch.S?
.macro	kernel_entry
	/* The first thing to do in handling an exception: saving cpu state, notably
	registers x0 - x30, to the stack. */
	sub	sp, sp, #S_FRAME_SIZE  //  subtracts from `sp` the size of total stored registers (#S_FRAME_SIZE) ...
	// ... and then fills the stack space
	/* TODO: your code here */

	/* to support multitasking, we need to save per-task elr and spsr. do that
	by using x22/x23 as temporary regs */
mrs x0, CurrentEL /* TODO: replace this */
mrs x0, CurrentEL /* TODO: replace this */

	stp	x30, x22, [sp, #16 * 15] 	
	str	x23, [sp, #16 * 16]
	// at this point, the stack content looks: 
	//	(lower addr) ...x28,29,x30,elr_el1,spsr_el1 (higher addr)
	.endm

/* -------------------------------------------------------------------------- */
// quest: "two preemptive printers"
.macro	kernel_exit
	/* A mirror of kernel_entry (above), to be called as the last thing of an
	exception handler */
	// first restore elr and spsr. do that via x22/x23 (cf above)
ldr x23, [sp] /* TODO: replace this */
	// also restore x30
ldp x30, x22, [sp] /* TODO: replace this */

	msr	elr_el1, x22			
	msr	spsr_el1, x23

	// restores the CPU state by copying back the values of x0 - x30
	ldp	x0, x1, [sp, #16 * 0]
	ldp	x2, x3, [sp, #16 * 1]
	ldp	x4, x5, [sp, #16 * 2]
	ldp	x6, x7, [sp, #16 * 3]
	ldp	x8, x9, [sp, #16 * 4]
	ldp	x10, x11, [sp, #16 * 5]
	ldp	x12, x13, [sp, #16 * 6]
	ldp	x14, x15, [sp, #16 * 7]
	ldp	x16, x17, [sp, #16 * 8]
	ldp	x18, x19, [sp, #16 * 9]
	ldp	x20, x21, [sp, #16 * 10]
	ldp	x22, x23, [sp, #16 * 11]
	ldp	x24, x25, [sp, #16 * 12]
	ldp	x26, x27, [sp, #16 * 13]
	ldp	x28, x29, [sp, #16 * 14]
	/* TODO: your code here */
	eret	// returns to the interrupted execution (e.g. still at EL1)
	.endm

/* -------------------------------------------------------------------------- */

/*  	
	**An exception vector** (or handler) is a piece of code the CPU will
	 execute when a specific exception happens. "*These would normally be branch
	 instructions that direct the core to the full exception handler.*" (the
	 ARM64 manual). 

	 The ARM64 hardware mandates: each exception vector can occupy `0x80` bytes
	 maximum (thus `.align 7` in the asm code). 

	For code clarity, we are not going to handle exceptions right inside the
	exception vector. Instead, we make each vector a branch instruction (`b
	\label`) that jumps to a label provided for the macro as `label` argument.
*/
 

/* 	
	The vector table for EL1. 

	**A vector table** is an array of exception vectors. Each exception level
	(EL) has its own vector table.     

	Purpose: to handle exceptions *taken from* EL0 (user programs) or EL1 (the
	kernel's own execution). ``vectors'' below is set as vector table for EL1 by
	irq_vector_init 

	the kernel defines 16 exception handlers: 4 types [sync, irq, fiq, serror]
	    for each of the CPU 4 execution states [EL1t, EL1h, EL0_64, EL0_32]. 
*/		
.align	11
.globl vectors 
// quest: "two preemptive printers"
vectors:
	//  EL1t -- Exception happens when CPU is at EL1 while the stack pointer (SP) was set to be shared with EL0
	ventry	sync_invalid_el1t			// Synchronous EL1t
	ventry	irq_invalid_el1t			// IRQ EL1t
	ventry	fiq_invalid_el1t			// FIQ EL1t
	ventry	error_invalid_el1t			// Error EL1t

	// EL1h -- Exception happens at EL1 at the time when a dedicated SP was allocated for EL1.
	//  		This is the mode that our kernel is currently using
	ventry	sync_invalid_el1h			// Synchronous EL1h
	// IRQ EL1h  
ventry irq_invalid_el1t /* TODO: replace this */
	ventry	fiq_invalid_el1h			// FIQ EL1h
	ventry	error_invalid_el1h			// Error EL1h

	// EL0_64 -- Exception is taken from EL0 executing in 64-bit mode. 
	//		The exceptions caused in 64-bit user programs
	ventry	sync_invalid_el0_64			// Synchronous 64-bit EL0
	ventry	irq_invalid_el0_64			// IRQ 64-bit EL0
	ventry	fiq_invalid_el0_64			// FIQ 64-bit EL0
	ventry	error_invalid_el0_64			// Error 64-bit EL0

	// EL0_32 -- Exception is taken from EL0 executing in 32-bit mode
	//		The exceptions caused in 32-bit user programs
	ventry	sync_invalid_el0_32			// Synchronous 32-bit EL0
	ventry	irq_invalid_el0_32			// IRQ 32-bit EL0
	ventry	fiq_invalid_el0_32			// FIQ 32-bit EL0
	ventry	error_invalid_el0_32			// Error 32-bit EL0

/* ---------------------------- end of EL1 vectors ---------------------------- */

el1_irq:
	kernel_entry 
	bl	handle_irq
	kernel_exit 

/* ------ "default" entries, behavior: print error msg & hang ----*/
sync_invalid_el1t:
	handle_invalid_entry  SYNC_INVALID_EL1t

irq_invalid_el1t:
	handle_invalid_entry  IRQ_INVALID_EL1t

fiq_invalid_el1t:
	handle_invalid_entry  FIQ_INVALID_EL1t

error_invalid_el1t:
	handle_invalid_entry  ERROR_INVALID_EL1t

sync_invalid_el1h:
	handle_invalid_entry  SYNC_INVALID_EL1h

fiq_invalid_el1h:
	handle_invalid_entry  FIQ_INVALID_EL1h

error_invalid_el1h:
	handle_invalid_entry  ERROR_INVALID_EL1h

sync_invalid_el0_64:
	handle_invalid_entry  SYNC_INVALID_EL0_64

irq_invalid_el0_64:
	handle_invalid_entry  IRQ_INVALID_EL0_64

fiq_invalid_el0_64:
	handle_invalid_entry  FIQ_INVALID_EL0_64

error_invalid_el0_64:
	handle_invalid_entry  ERROR_INVALID_EL0_64

sync_invalid_el0_32:
	handle_invalid_entry  SYNC_INVALID_EL0_32

irq_invalid_el0_32:
	handle_invalid_entry  IRQ_INVALID_EL0_32

fiq_invalid_el0_32:
	handle_invalid_entry  FIQ_INVALID_EL0_32

error_invalid_el0_32:
	handle_invalid_entry  ERROR_INVALID_EL0_32

/* ---------------------------- ret_from_fork ---------------------------- */

// quest "two cooperative printers"
.globl ret_from_fork
// the **first** piece of code executed by a newly created process. 
// only executed once throughout a task's lifetime
// NB: despite the name "fork", we are not doing fork() as in Linux/Unix
ret_from_fork:
	bl	leave_scheduler
	/* 	Explanation: copy_process() saves `fn` (the process's main function) and
	`arg` (the argument passed to the process) to`task_struct.x19` and `x20`.
	When switching to a new task, the kernel restores `fn` and `arg` from
	`task_struct` to `x19` and `x20`. Below, `ret_from_fork` calls the function
	stored in `x19` register with the argument stored in `x20`. */
mov x0, x0 /* TODO: replace this */
	// call a kern task's main func
blr x0 /* TODO: replace this */
	// Below: 
	// the kern task returns from main func... what happen next? 
	// have students think about it. also exercise use of gdb
	// what's on stack now? empty.. nowhere to "return to". so fall through to 
	// below ... trapped in the err_hang inf loop (without it, the task will 
	// execute arbitrary code) 
	// the task is still "alive", but doing nothing just burning cpu cycles.
	// other tasks may still run if preemptive scheduling is on
	/*
		>>> bt
		#0  err_hang () at entry.S:212
		#1  0x0000000000085e8c in ret_from_fork () at entry.S:208
		Backtrace stopped: previous frame identical to this frame (corrupt stack?)
	*/
.globl err_hang
err_hang: b err_hang
