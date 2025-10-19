// #define K2_DEBUG_VERBOSE
#define K2_DEBUG_WARN

#include <stddef.h>
#include <stdint.h>

#include "plat.h"
#include "utils.h"
#include "sched.h"

// unittests.c
extern void test_ktimer(); 
extern void test_fb(); 
extern void test_kern_tasks_print(); 
extern void test_kern_tasks_donut(); 
extern void test_kern_task_mgmt(); 
extern void test_kern_reader_writer(); 
extern void donut(int x, int y); 	//donut.c

struct cpu cpus[NCPU]; 

// quest "two preemptive printers"
void kernel_main() {
	uart_init();
	init_printf(NULL, putc);	
	printf("------ kernel boot ------  core %d\n\r", cpuid());
	printf("build time (kernel.c) %s %s\n", __DATE__, __TIME__); // simplicity 
			
	paging_init(); 
	sched_init(); 	// must be before schedule() or timertick() 
	fb_init(); 		// reserve fb memory other page allocations
	sys_timer_init(); 		// kernel timer: delay, timekeeping...
	enable_interrupt_controller(0/*coreid*/);
	/* turn on cpu irq  */
	/* TODO: your code here */
	
	/* sched ticks alive. preemptive scheduler is on */
	/* TODO: your code here */
	
	/* now cpu is on its boot stack (boot.S) belonging to the idle task. 
	schedule() will jump off to kernel stacks belonging to normal tasks
	(i.e. init_task as set up in sched_init(), sched.c) */
	schedule(); 
	/* only when scheduler has no normal tasks to run for the current cpu,
	the cpu switches back to the boot stack and returns here */
    while (1) {
        /* don't call schedule(), otherwise each irq calls schedule(): too much
        instead, let timer_tick() throttle & decide when to call schedule() */
        V("idle task");
        asm volatile("wfi");
    }
}

/* the 1st task (other than "idle"), created by sched_init()
as the launchpad of various kernel tests, etc.  */
void init(int arg/*ignored*/) {
	int wpid; 
    W("entering init");

	// quest: "two cooperative printers"
	/* TODO: your code here */
	// quest: "two donuts"
	/* TODO: your code here */
	// test_kern_task_mgmt();
	// test_kern_reader_writer(); 

	while (1) {
		wpid = wait(0 /* does not care about status */); 
		if (wpid < 0) {
			W("init: wait failed with %d", wpid);
			panic("init: maybe no child. has nothing to do. bye"); 
		} else {
			W("wait returns pid=%d", wpid);
			/* a parentless task  */
		}
	}
}
