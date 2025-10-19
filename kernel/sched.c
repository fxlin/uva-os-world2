// #define K2_DEBUG_VERBOSE
// #define K2_DEBUG_INFO
#define K2_DEBUG_WARN

#include "plat.h"
#include "utils.h"
#include "sched.h"
#include "printf.h"
#include "spinlock.h"
#include "entry.h"

/* kernel_stacks[i]: kernel stack for task with pid=i.
WARNING: various kernel code assumes each kernel stack is page-aligned.
cf. ret_from_syscall (entry.S). if you modify the d/s below, keep that in mind.  */
static __attribute__ ((aligned (PAGE_SIZE)))
char kernel_stacks[NR_TASKS][THREAD_SIZE]; 

// used during boot, then used as the kern stacks for idle tasks
__attribute__ ((aligned (PAGE_SIZE))) 
char boot_stacks[NCPU][THREAD_SIZE];

struct task_struct *init_task; 
struct task_struct *task[NR_TASKS]; // normal tasks 
struct task_struct *idle_tasks[NCPU];  // per cpu, only scheduled when no normal tasks runnable

struct spinlock sched_lock = {.locked=0, .cpu=0, .name="sched"};

struct cpu cpus[NCPU]; 

static char *states[] = {
    [TASK_UNUSED]   "UNUSED  ",
    [TASK_RUNNING]  "RUNNING ",
    [TASK_SLEEPING] "SLEEP   ",
    [TASK_RUNNABLE] "RUNNABLE",
    [TASK_ZOMBIE]   "ZOMBIE  "};
    
struct task_struct *myproc(void) {      
    struct task_struct *p;
    /* need disable irq b/c: if right after mycpu(), the cur task moves to 
    a diff cpu, then cpu still points to a previous cpu and ->proc 
    is not this task but a diff one */
	push_off(); 
    p=mycpu()->proc; 
    pop_off(); 
	return p; 
};

extern void init(int arg); // kernel.c

/* must be called BEFORE any schedule() or timertick() occurs */
void sched_init(void) {
    for (int i = 0; i < NR_TASKS; i++) {
        task[i] = (struct task_struct *)(&kernel_stacks[i][0]); 
        BUG_ON((unsigned long)task[i] & ~PAGE_MASK);  // must be page aligned. see above
        memset(task[i], 0, sizeof(struct task_struct)); // zero everything
        initlock(&(task[i]->lock), "task");
        task[i]->state = TASK_UNUSED;
    }

    for (int i = 0; i < NCPU; i++) {
        idle_tasks[i] = (struct task_struct *)(&boot_stacks[i][0]); 
        cpus[i].proc = idle_tasks[i]; 
        initlock(&(idle_tasks[i]->lock), "idle"); // some code will try to grab
        snprintf(idle_tasks[i]->name, 10, "idle-%d", i); 
        idle_tasks[i]->pid = -1; // not meaningful. a placeholder
        /* when each cpu calls schedule() for the first time, they will 
        jump off the idle task to "normal" ones, saving cpu_context 
        (inc sp/pc) to idle_tasks[i] */
    }
    
    /* init task, will be picked up once cpu0 calls schedule() for the 1st time */
    init_task = task[0]; 
    init_task->state = TASK_RUNNABLE;
    init_task->cpu_context.x19 = (unsigned long)init; 
    init_task->cpu_context.pc = (unsigned long)ret_from_fork; // entry.S
    init_task->cpu_context.sp = (unsigned long)init_task + THREAD_SIZE; 

    init_task->credits = 0;
    init_task->priority = 2;
    init_task->flags = PF_KTHREAD;
    // init_task->mm = 0;  // nothing (kernel task) 
    init_task->chan = 0;
    init_task->pid = 0;
    safestrcpy(init_task->name, "init", 5);
}

/* return cpuid for the task currently on; 
-1 on no found or error
caller must hold sched_lock */
static int task_on_cpu(struct task_struct *p) {
    if (!p) {BUG(); return -1;}
    for (int i = 0; i < NCPU; i++)
        if (cpus[i].proc == p)
            return i; 
    return -1; 
}

/* the scheduler, called by tasks or irq. invoked for both cooperative 
    (via yield()) and preemptive scheduling (via timer interrupt).
    caller must NOT hold sched_lock */
// quest: "two cooperative printers"
void schedule() {
    V("cpu%d schedule", cpuid());

	int next, max_cr;
    int cpu, oncpu;
    
    /* this cpu run on the kernel stack of task "cur"; our design 
    ensures that "cur" CANNOT be picked by other cpus */
	struct task_struct *p, *cur=myproc();
    int has_runnable; 

    acquire(&sched_lock); 
    cpu = cpuid();  // holding sched_lock, the cur process wont mirgrate across cpus

	while (1) {
		max_cr = -1; 
		next = 0;
        has_runnable = 0; 

		/* Among all RUNNABLE tasks (plus the cur task, if it's RUNNING), 
           find a task w/ maximum credits. */
		for (int i = 0; i < NR_TASKS; i++){
			p = task[i]; BUG_ON(!p);
            // if task is active on other cpu, dont touch
            oncpu = task_on_cpu(p); 
            if (oncpu != -1 && oncpu != cpu) 
                continue;
			if ((p == cur && p->state == TASK_RUNNING)
                || p->state == TASK_RUNNABLE) {
                has_runnable = 1; 
                /* NB: p->credits protected by sched_lock */
                V("cpu%d pid %d credits %ld", cpu, i, p->credits);
				if (p->credits > max_cr) { max_cr = p->credits; next = i; }
			}
		}        
		if (max_cr > 0) {
            I("cpu%d picked pid %d state %s credits %ld", cpu, next, 
                states[task[next]->state], p->credits);
switch_to(0); /* TODO: replace this */
			break;
        }

		/* No task can run ... */
        if (has_runnable) { 
            /* reason1: insufficient credits. recharge for all & retry scheduling */
            for (int i = 0; i < NR_TASKS; i++) {
                p = task[i]; BUG_ON(!p);
                if (p->state != TASK_UNUSED) {
                    /* NB: p->credits/priority protected by sched_lock */
                    p->credits = (p->credits >> 1) + p->priority;  // per priority
                }                
            }
        } else { /* reason2: no normal tasks RUNNABLE (inc. cur task) */
            V("cpu%d nothing to run. switch to idle", cpu); 
            #ifdef K2_DEBUG_VERBOSE
            procdump(); 
            #endif
            /* if cpu already on idle task, this will do nothing */
switch_to(0); /* TODO: replace this */
            break;
        }
	}
    release(&sched_lock);
    /* leave the scheduler: the primary path  */
}

/* 
    leave the scheduler: the secondary path

    This function is needed b/c when a task is "switched to" for the first time,
    the task starts to execute from ret_from_fork instead of the instruction
    right after the callsite to cpu_switch_to(), (see comments in switch_to()).
    To balance the irq_disable/enable, ret_from_fork must call leave_scheduler()
    below */
void leave_scheduler(void) {
    release(&sched_lock);
    enable_irq(); // new task must turn on irq. cf timer_tick() comments
}

/* voluntarily reschedule; gives up all remaining schedule credits
only called from tasks */
// quest: "fast/slow donuts"
void yield(void) {    
    struct task_struct *p = myproc(); 
    acquire(&sched_lock); p->credits = 0; release(&sched_lock);
    schedule();
}

/* caller must hold sched_lock, and not holding next->lock
called when preemption is disabled, so the cur task wont lose cpu */
// quest: "two cooperative printers"
void switch_to(struct task_struct * next) {
	struct task_struct * prev; 
    struct task_struct *cur; 

    cur = myproc(); BUG_ON(!cur); 
	if (cur == next) 
		return; 

	prev = cur;
	mycpu()->proc = next;

	if (prev->state == TASK_RUNNING) // preempted 
		prev->state = TASK_RUNNABLE; 
	next->state = TASK_RUNNING;

    /*
        Here is where context switch happens.

        after cpu_switch_to(), the @prev's cpu_context.pc points to the 
        instruction right after cpu_switch_to(). this is where the @prev task 
        will resume in the future. The corresponding instruction is shown as 
        the arrow below:

            cpu_switch_to(prev, next);
            80d50:       f9400fe1        ldr     x1, [sp, #24]
            80d54:       f94017e0        ldr     x0, [sp, #40]
            80d58:       9400083b        bl      82e44 <cpu_switch_to>
        ==> 80d5c:       14000002        b       80d64 <switch_to+0x58>

        cpu_switch_to() does not need task::lock, cf "locking protocol" on the top
    */

    /* below: cpu_switch_to() in switch.S. it will branch to next->cpu_context.pc */
cpu_switch_to(0, 0); /* TODO: replace this */
}

#define CPU_UTIL_INTERVAL 10  // cal cpu measurement every X ticks

/* Called by handle_generic_timer_irq(), i.e. timer irq handler, with irq 
    automatically turned off by hardware. irq status can be checked by 
    is_irq_masked() */
void timer_tick() {
    struct task_struct *cur = myproc();
    struct cpu* cp = mycpu(); 

    if (cur) { // update task::credits, decide if schedule() is needed
        V("enter timer_tick cpu%d task %s pid %d", cpuid(), cur->name, cur->pid);
        if (cur->pid>=0 && cur->state == TASK_RUNNING) // not "idle" (pid -1), and running
            cp->busy++; 

        // calculate cpu util %     quest: hide this until later lab
        if ((cp->total++ % CPU_UTIL_INTERVAL) == CPU_UTIL_INTERVAL - 1) {
            cp->last_util = cp->busy * 100 / CPU_UTIL_INTERVAL; 
            cp->busy = 0; 
            V("cpu%d util %d/100, cur %s", cpuid(), cp->last_util, cur->name); 
            #if K2_ACTUAL_DEBUG_LEVEL <= 20     // "V"
            extern void procdump(void);
            if (cpuid()==0)
                procdump();
            #endif
        }

        acquire(&sched_lock); 
        if (cur->pid>=0 && --cur->credits > 0) { 
            // let "cur" task to continue execution 
            V("leave timer_tick. no resche");
            release(&sched_lock); return;
        }
        cur->credits=0;
        release(&sched_lock);
    }

    /* At this moment, irq is disabled (DAIF.I is set), until it is only enabled 
       (restored from SPSR) by kernel_exit which does `eret`. However, if 
       schedule() below switches to a new task, which runs for its first time and 
       does NOT proceed to execute kernel_exit(), then irq will be left disabled 
       forever -- no more scheduling.
       
       That is why a new task starts from ret_from_fork() which calls 
       leave_scheduler() to enable irq. */
    
	schedule();

    V("leave timer_tick cpu%d task %s pid %d", cpuid(), cur->name, cur->pid);
	
    /* irq disabled until kernel_exit, in which eret will restore the 
       DAIF.I flag from spsr, which sets irq on. */
}

/* -------------  sleep() & wakeup() etc  -------------------- */

/* Design patterns for sleep() & wakeup() 

sleep() always needs to hold a lock (lk). inside sleep(), once the calling
task grabs sched_lock (i.e. no other tasks can change their p->state), lk is
released

ONLY USE sched_lock to serialize task A/B is not enough wakeup() does NOT
need to hold lk. if that's the case, it's possible: task B: sleep(on chan) in
a loop; after it wakes up (no schedlock; only lk), before it calls sleep()
again, task A calls wakeup(chan), taking schelock and wakes up no task -->
wakeup is lost So our kernel cannot help on this case

to avoid the above, task A calling wakeup() must hold lk beforehand. b/c of
this, only after task B inside sleep() rls lk, task A can proceed to
wakeup(). inside wakeup(), task A is further serialized on schedlock, which
must wait until that task B has completely changed its p->state and is moved
off the cpu */

/* Wake up all processes sleeping on chan. Only change p->state; wont call
schedule() return # of tasks woken up.
Caller must hold sched_lock  */
// quest: "wordsmith"
static int wakeup_nolock(void *chan) {
    struct task_struct *p;
    int cnt = 0; 
    // V("chan=%lx", (unsigned long)chan);
	for (int i = 0; i < NR_TASKS; i ++) {
		p = task[i]; 
        // NB: it's possible that p == cur and should be woken up
        if (p->state == TASK_UNUSED) continue; 
        if (p->state == TASK_SLEEPING && p->chan == chan) {            
            /* TODO: your code here */
            I("wakeup cpu%d chan=%lx pid %d", cpuid(),
                (unsigned long)p->chan, p->pid);
        }
    }
    return cnt; 
}

/* Must be called WITHOUT sched_lock 
Called from irq (many drivers) or task
return # of tasks woken up */
// quest: "wordsmith"
int wakeup(void *chan) {
    int cnt; 
    acquire(&sched_lock);     
    cnt = wakeup_nolock(chan); 
    release(&sched_lock);
    return cnt; 
}

/* Atomically release "lk" and sleep on chan.
Reacquires lk when awakened.
Called by tasks with @lk held */
// quest: "wordsmith"
void sleep(void *chan, struct spinlock *lk) {
    struct task_struct *p = myproc();

    /*
     * Must acquire sched_lock in order to
     * change p->state and then call schedule().
     * 
     * this is useful for many drivers where caller acquire
     * the same "lk" used to sleep() prior to calling wakeup() 
     * (e.g. lk protects the same buffer, cf pl011.c)
     *
     * Once we hold sched_lock, we can be
     * guaranteed that we won't miss any wakeup (meaning that another task 
     * calling wakeup() w/ holding lk)
     * b/c wakeup() can only 
     * start to wake up tasks after it locks sched_lock.
     * so it's okay to release lk.
     * 
     * Corner case: lk==sched_lock, which is already held by cur task. the right
     * behavior of sleep(): keep sched_lock and switch to idle task, which later
     * will release the lock
     */
    if (lk != &sched_lock) {
        acquire(&sched_lock);
        release(lk);
    }

    I("sleep chan=%lx pid %d", (unsigned long)chan, p->pid);

    /* Go to sleep. */
    /* TODO: your code here */

    /* although the task has not used up the current tick, bill it regardless.
    thus this task will be disadvantaged in future scheduling  */
    p->credits --; 

    /* switch the cpu away from the current kern stack to the idle task, which we
    know exists for sure. the idle task will return from the schedule() and 
    rls sched_lock. the next timertick will call schedule() and switch 
    to a normal task (if any)  */
struct task_struct *idle = 0; /* TODO: replace this */
    mycpu()->proc = idle; 
    cpu_switch_to(p, idle);  
    
    /* cpu_switch_to() back here when the cur task is woken up. 
    it now has sched_lock.  */

    /* Tidy up. */
    p->chan = 0;

    if (lk != &sched_lock) {
        release(&sched_lock); 
        acquire(lk); 
        /* This task (T1) shall first release sched_lock before reacquiring 
        the original lock (lk). This avoids deadlock with another task T2 calling wakeup(). 
        Ex: 
        - T2 called wakeup() while holding lk (which drivers often do); 
        - T2 released sched_lock but still has lk 
        - T1 schedule in (after cpu_switch_to above), holding sched_lock;
        - T1 tries to reacquire lk (before releasing sched_lock)
        - T2 has lk, but cannot run b/c T1 has sched_lock -- deadlock         
            cf unittests.c do_write()
        */
    } /* else keep holding sched_lock */
}

/* Pass p's abandoned children to init. (ie direct reparent to initprocess)
return # of children reparanted
Caller must hold sched_lock. */
static int reparent(struct task_struct *p) {
    struct task_struct **child;
    int cnt = 0; 
    for (child = task; child < &task[NR_TASKS]; child++) {
        BUG_ON(!(*child));
        if ((*child)->state == TASK_UNUSED) continue;
        if ((*child)->parent == p) {
            (*child)->parent = init_task;
            cnt ++; 
        }
    }
    return cnt; 
}

static void freeproc(struct task_struct *p); 

/* Wait for a child process to exit and return its pid.
    Return -1 if this process has no children. 
    addr=0 a special case, dont care about status
    --- "addr" ignored for lab2 */
int wait(uint64 addr /*dst user va to copy status to */) {
    struct task_struct **pp;
    int havekids, pid;
    struct task_struct *p = myproc();

    I("pid %d (%s) entering wait()", p->pid, p->name);

    /* make sure the (zombie) child is done with exit() and has been 
    switched away from (so that no cpu uses the zombie's kern stack) 
    cf exit_process() below */
    acquire(&sched_lock); 

    for (;;) {
        // Scan through table looking for exited children.  pp:child
        havekids = 0;
        for (pp = task; pp < &task[NR_TASKS]; pp++) {
            struct task_struct *p0 = *pp; BUG_ON(!p0); 
            if (p0->state == TASK_UNUSED) continue; 
            if (p0->parent == p) {
                havekids = 1;
                if (p0->state == TASK_ZOMBIE) {
                    // Found one.
                    pid = p0->pid;
                    I("found zombie pid=%d", pid); 
                    freeproc(p0);       // will mark the task slot as unused                    
                    release(&sched_lock); 
                    // the task slot now may be reused
                    return pid;
                }
            }
        }
        
        // No point waiting if we don't have any children.
        if (!havekids) {
            release(&sched_lock);
            return -1;
        }

        I("pid %d sleep on %lx", p->pid, (unsigned long)&sched_lock);
        sleep(p, &sched_lock); // sleep on own task_struct
        I("pid %d wake up from sleep. p->chan %lx state %d", p->pid, 
            (unsigned long)p->chan, p->state);
    }
}

/* Becomes a zombie task and switch the cpu away from it 
only when parent calls wait() this zombie task successfully, the zombie's 
kernel stack (and task_struct on it) will be recycled. */
// quest: "kill a donut"
void exit_process(int status) {
    struct task_struct *p = myproc();

    I("pid %d (%s): exit_process status %d", p->pid, p->name, status);

    if (p == init_task)
        panic("init exiting");

    /* This prevents the parent from checking & recycling this zombie until 
    the cpu moves away from the zombie's stack (see below) */
    acquire(&sched_lock); 

    /* Give any children to init. */
    if (reparent(p)) 
        wakeup_nolock(init_task);

    /* Parent might be sleeping in wait(). */
    wakeup_nolock(p->parent); 
    p->xstate = status;
    p->state = TASK_ZOMBIE;
    
    V("exit done. will switch away...");
    /* now the woken parent still CANNOT recycle this zombie b/c we hold
    sched_lock  */
    
    /* switch the cpu away from zombie's kern stack to the idle task, which we
    know exists for sure. the next timertick will call schedule() and switch 
    to a normal task (if any) */
    /* TODO: your code here */

    /* the "switch-to" task will resume from the schedule()'s exit path, which
    will release sched_lock after sched_lock is released, the parent can proceed
    to recycle the zombie's kern stack (& task_struct), which is no longer used
    by any cpu  */
    panic("zombie exit");
}

/* Destroys a task: task_struct, kernel stack, etc. free a proc structure and
    the data hanging from it, including user & kernel pages. 

    sched_lock must be held.  p->lock must be held */
static void freeproc(struct task_struct *p) {
    BUG_ON(!p); V("%s entered. pid %d", __func__, p->pid);

    p->state = TASK_UNUSED; // mark the slot as unused
    // o need to zero task_struct, which is among the task's kernel page
    // FIX: since we cannot recycle task slot now, so we dont dec nr_tasks ...
    p->flags = 0; 
    p->killed = 0; 
    p->credits = 0; 
    p->chan = 0; 
    p->pid = 0; 
    p->xstate = 0; 
}

/* Print a process listing to console.  For debugging.
Runs when user types ^P on console.
No lock to avoid wedging a stuck machine further. */
void procdump(void) {
    struct task_struct *p;
    char *state;

    printf("\t %5s %10s %10s %20s\n", "pid", "state", "name", "sleep-on");

    for (int i = 0; i < NR_TASKS; i++) {
        p = task[i];
        if (p->state == TASK_UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        printf("\t %5d %10s %10s %20lx\n", p->pid, state, p->name, 
               (unsigned long)p->chan);
    }
    
    extern unsigned paging_pages_used, paging_pages_total; // alloc.c
	printf("paging mem: used %u total %u (%u/100)\n", 
		paging_pages_used, paging_pages_total, 
        paging_pages_used*100/(paging_pages_total));
}

/* -------------  fork related  -------------------- */

static int lastpid=0; // a hint for the next free tcb slot. slowdown pid reuse for dbg ease

/* For creating both user and kernel tasks

    return pid on success, <0 on err

    clone_flags: PF_KTHREAD for kernel thread, PF_UTHREAD for user thread
    fn: task func entry. only matters for PF_KTHREAD.
    arg: arg to kernel thread; or stack (userva) for user thread
    name: to be copied to task->name[]. if null, copy parent's name
*/
// quest "two cooperative printers"
int copy_process(unsigned long clone_flags, unsigned long fn, unsigned long arg,
    const char *name) {
	struct task_struct *p = 0, *cur=myproc(); 
    int i, pid; 

	acquire(&sched_lock);	
	// find an empty tcb slot
	for (i = 0; i < NR_TASKS; i++) {
        pid = (lastpid+1+i) % NR_TASKS; 
		p = task[pid]; BUG_ON(!p); 
		if (p->state == TASK_UNUSED)
			{V("alloc pid %d", pid); lastpid=pid; break;}
	}
	if (i == NR_TASKS) 
		{release(&sched_lock); return -1;}

	memset(p, 0, sizeof(struct task_struct));
	initlock(&p->lock, "proc");

	acquire(&p->lock);	
    acquire(&cur->lock);	

    // load fn/arg to cpu context. cf ret_from_fork
    /* TODO: your code here */

    // also inherit task name
    if (name)
        safestrcpy(p->name, name, sizeof(p->name));
    else 
	    safestrcpy(p->name, cur->name, sizeof(cur->name));

	p->flags = clone_flags;
	p->credits = p->priority = cur->priority;
	p->pid = pid; 

	// @page is 0-filled, many fields (e.g. mm.pgd) are implicitly init'd

    // prep new task's scheduler context: assign values to the pc/sp of new
    // task's cpu_context
	/* TODO: your code here */
	
    release(&cur->lock);
	release(&p->lock);

 	p->parent = cur;
	// the last thing: change the task's state so that the scheduler can pick up
    // the task to run in the future
	/* TODO: your code here */
	
	release(&sched_lock);

	return pid;
}
