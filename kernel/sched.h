#ifndef _SCHED_H
#define _SCHED_H

// quest: figure this out
#define THREAD_CPU_CONTEXT 0 // offset of cpu_context in task_struct

#ifndef __ASSEMBLER__

#define THREAD_SIZE PAGE_SIZE // kernel stack size per task

#define TASK_UNUSED 0  // unused tcb slot
#define TASK_RUNNING 1 // task on a cpu
#define TASK_SLEEPING 2
#define TASK_ZOMBIE 3
#define TASK_RUNNABLE 4 // can run but not on any cpu
/* TODO: define more task states (as constants) below, e.g. TASK_WAIT */

#define PF_KTHREAD		 0x2	// kern thread
#define PF_UTHREAD	 	 0x4	// user thread (p->mm shared with other user tasks)

extern struct task_struct * task[NR_TASKS];

/* a task's (partial) cpu regs for context switches in scheduling 
x0-x7 func call arguments; x9-x15 caller saved; x19-x29 callee saved */
struct cpu_context {	
	unsigned long x19;
	unsigned long x20;
	unsigned long x21;
	unsigned long x22;
	unsigned long x23;
	unsigned long x24;
	unsigned long x25;
	unsigned long x26;
	unsigned long x27;
	unsigned long x28;
	unsigned long fp;
	unsigned long sp;
	unsigned long pc;
}; // 13 regs

struct user_page {
	unsigned long phys_addr;
	unsigned long virt_addr; // user va
};

/* a task's full cpu regs when taking exceptions (EL1->EL1, EL0->EL1) */
struct trapframe {
	unsigned long regs[31];
	unsigned long sp;
	unsigned long pc;
	unsigned long pstate;
  unsigned long stackframe[2]; // for unwinder
};

#include "spinlock.h"

/* A user task's VM. 
  A VM can be shared by multi user tasks kernel thread has no such a thing,
  task_struct::mm=0 will be allocated in a static table, cf mm_table TODO: the
  size grows with MAX_TASK_XXX_PAGES, could be problem for larger user programs
  in the future...
 */
struct mm_struct {
  /* # of task_structs refers to this mm_struct. 0 means invalid. 
    accessed using atomic intrinsics (instead of a spinlock), in order to 
    avoid deadlock between mm_struct::lock and sched_lock. cf sched.c comments
    on the lock protocol */
	int ref; 
  
  struct spinlock lock; // to protect everything below 

	unsigned long pgd;	// pa. this is loaded to ttbr0 (user va)
	
	unsigned long sz, codesz; 	// for a user task, VA [0, sz) covers its code,data,&heap. [0,codesz) covers code &data. not page aligned
	int user_pages_count;
	int kernel_pages_count;
	/* which kernel pages are used by this task, e.g. those for pgtables.  PA */
	unsigned long kernel_pages[MAX_TASK_KER_PAGES]; 	
};

/* the metadata describing a task */
//quest:fast/slow donuts. Understand below
struct task_struct {
    /* private to the task, no task->lock needed */
    struct cpu_context cpu_context; // MUST COME FIRST. register values.
    struct file *ofile[NOFILE];     // Open files
    struct inode *cwd;              // Current directory
    char name[16];                  // Process name (debugging)
    struct mm_struct *mm;           // =0 for kernel thread. for user threads, multi task_structs may share a mm_struct
    unsigned long flags;
    long preempt_count; // cf: preempt_enable()  TO DELETE

    struct spinlock lock;
    // the lock above protects members below
    int killed; // If non-zero, have been killed. Checked by entry.S.
    /* killed protected by p->lock (instead of sched_lock) b/c it's not tied to schedule() logic and is
    examined in many places */
    int pid; // still need this, ease of debugging...

    /* the global sched_lock is needed for the following, b/c they are only examined
    by schedule() & friends */
    int state;                  // task state, e.g. TASK_RUNNING. TASK_UNUSED if task_struct is invalid
    long credits;               // schedule "credits". dec by 1 for each timer tick; upon 0, calls schedule(); schedule() picks the task with most credits
    long priority;              // when kernel schedules a new task, the kernel copies the task's  `priority` value to `credits`. Regulate CPU time the task gets relative to other tasks
    int xstate;                 // Exit status to be returned to parent's wait
    void *chan;                 // If non-zero, sleeping on chan
    struct task_struct *parent; // Parent process
};

/* use the code below to check struct size at compile time
https://stackoverflow.com/questions/11770451/what-is-the-meaning-of-attribute-packed-aligned4
char (*__kaboom)[sizeof(struct task_struct)] = 1;  */

/* bottom half a page; make sure the top half enough space for ker stack... */
_Static_assert(sizeof(struct task_struct) < 1200);	// 1408 seems too big, corrupts tjhe stack

// --------------- cpu related ----------------------- //
struct cpu {
    struct task_struct *proc; // The process running on this cpu. never null as each core has an idle task
    int noff;                 // Depth of push_off() nesting.
    int intena;               // Were interrupts enabled before push_off()?
    /* below: in # of ticks */
    int busy;            // # of busy ticks in current measurement interval
    int last_util;       // out of 100, cpu util in the past interval
    unsigned long total; // since cpu boot
};
extern struct cpu cpus[NCPU];		// sched.c

extern int cpuid(void); // irq must be disabled
static inline struct cpu* mycpu(void) {return &cpus[0];}; // uniprocessor (UP)

// --------------- fork related ----------------------- // 
#define PSR_MODE_EL0t	0x00000000
#define PSR_MODE_EL1t	0x00000004
#define PSR_MODE_EL1h	0x00000005
#define PSR_MODE_EL2t	0x00000008
#define PSR_MODE_EL2h	0x00000009
#define PSR_MODE_EL3t	0x0000000c
#define PSR_MODE_EL3h	0x0000000d
#endif		// ! __ASSEMBLER__

#endif

