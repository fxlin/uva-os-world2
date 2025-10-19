// #define K2_DEBUG_VERBOSE
#define K2_DEBUG_INFO
// #define K2_DEBUG_WARN

#include "plat.h"
// #include "mmu.h"
#include "utils.h"
#include "printf.h"
#include "spinlock.h"
#include "sched.h"

// Use of harware timers 
// - Per-core "arm generic timers": driving scheduler ticks
// - Chip-level "arm system timer": timekeeping, virtual timers w/ callbacks,
//   sys_sleeep(), etc. 
// It's possible to only use "arm generic timer" for all these purposes like
// xv6 (+ software tricks like sched tick throttling, distinguishing timers on
// different cpus, etc) which however result in more complex design. 

// Sched ticks should occur periodically, but not too often -- otherwise 
// numerous nested calls to schedule() will exhaust & corrupt the kernel state. 
// So be careful when you change the HZ below 
// 10Hz assumed by some code in usertests.c
#define SCHED_TICK_HZ	10
// sys_sleep() based on sched ticks -- too coarse grained for certain apps, e.g.
// NES emulator relies on sys_sleep() to sleep/wake at 60Hz for its rendering.

/* sched interval, for arm generic timer. 
when on qemu, arm generic timer is at 1MHz by default
on rpi3, it's also 1MHz. */
int interval = (1 * 1000 * 1000 / SCHED_TICK_HZ);

////////////////////////////////////////////////////////////////////////////////

/**
 *  Arm generic timers. Each core has its own instance. 
 *
 *  Here, the physical timer at EL1 is used with the TimerValue views.
 *  Once the count-down reaches 0, the interrupt line is HIGH until
 *  a new timer value > 0 is written into the CNTP_TVAL_EL0 system register.
 *
 *  Read: 
 *  https://fxlin.github.io/p1-kernel/exp3/rpi-os/#arms-generic-hardware-timer
 * 
 *  Reference: AArch64-referenc-manual p.2326 at
 *  https://developer.arm.com/docs/ddi0487/ca/arm-architecture-reference-manual-armv8-for-armv8-a-architecture-profile
 */

static void generic_timer_reset(int intv) {	
	asm volatile("msr CNTP_TVAL_EL0, %0" : : "r"(intv));  // TVAL is 32bit, signed
}

void generic_timer_init (void) {
  	// writes 1 to the control register (CNTP_CTL_EL0) of the EL1 physical timer
 	// 	CTL: control register
	// 	CNTP_XXX_EL0: this is for EL1 physical timer
	// 	_EL0: timer accessible to both EL1 and EL0
	asm volatile("msr CNTP_CTL_EL0, %0" : : "r"(1));

	generic_timer_reset(interval);	// kickoff 1st time firing
}

//quest: "two preemptive printers"
void handle_generic_timer_irq(void)  {
	// scheduling at SCHED_TICK_HZ could be too frequent. can throttle like: 
	// if (ticks % 10 == 0 || woken)
	
	/* 	Reset the timer before calling timer_tick() (which calls 
	schedule()..), not after it. Otherwise, enable_irq() inside 
	timer_tick() will trigger a new timer irq IMMEDIATELY (looks like hw 
	checks for the generic timer's condition whenever DAIF is set? or the 
	behavior of qemu?). As a result, timer_irq handler will be called 
	back to back, corrupting the kernel stack  */

	generic_timer_reset(interval);
	
	// drive the scheduler
	/* TODO: your code here */
}

////////////////////////////////////////////////////////////////////////////////

/* 
	Rpi3's "system Timer". 
	- Support "virtual timers" and timekeeping (current_time(), sys_sleep()). 
	- Efficient. No periodic interrupts. Instead, set & fire on demand. 
	- IRQ always routed to core 0.

	cf: test_ktimer() on how to use.

	NB: in earlier qemu (<5), emulation for system timer is incomplete --
	cannot fire interrupts. 
	https://fxlin.github.io/p1-kernel/exp3/rpi-os/#fyi-other-timers-on-rpi3		

*/
#if defined(PLAT_RPI3) || defined(PLAT_RPI3QEMU)
#define N_TIMERS 20 	// # of vtimers
#define CLOCKHZ	1000000	// rpi3 use 1MHz clock for system counter. 

#define TICKPERSEC (CLOCKHZ)
#define TICKPERMS (CLOCKHZ / 1000)
#define TICKPERUS (CLOCKHZ / 1000 / 1000)

// return # of ticks (=us when clock is 1MHz)
// NB: use current_time() below to get converted time
static inline unsigned long current_counter() {
	// assume these two are consistent, since the clock is only 1MHz...
	return ((unsigned long) get32(TIMER_CHI) << 32) | get32(TIMER_CLO); 
}

//////////////////////////////
//  delay, timekeeping 

// # of cpu cycles per ms, per us. measured. will be tuned during boot
#ifdef PLAT_RPI3
/*  cache on */
// static unsigned int cycles_per_ms = 602409;
// static unsigned int cycles_per_us = 599; 
/* cache off, much slower */
static unsigned int cycles_per_ms = 5011;
static unsigned int cycles_per_us = 5; 
#elif defined(PLAT_RPI3QEMU)
/* cache off: */
static unsigned int cycles_per_ms = 434782;
static unsigned int cycles_per_us = 434; 
#endif

// use sys timer to measure: # of cpu cycles per ms 
// can be slow if cache is off
__attribute__((unused))
static void sys_timer_tune_delay() {
	unsigned long cur0 = current_counter(), ms, us; 
	unsigned long ncycles = 100 * 1000 * 1000; 	// run 100M cycles. delay should >1ms
	delay(ncycles); 	
	us = (current_counter() - cur0) / TICKPERUS; 
	ms = us / 1000; 

	cycles_per_us = ncycles / us; 
	cycles_per_ms = ncycles / ms; 
	I("cycles_per_us %u cycles_per_ms %u", cycles_per_us, cycles_per_ms);
}

void ms_delay(unsigned ms) {
	BUG_ON(!cycles_per_ms);
	delay(cycles_per_ms * ms); 
}

void us_delay(unsigned us) {
	BUG_ON(!cycles_per_us);
	delay(cycles_per_us * us); 
}

// can only be called after va is on, timers are init'd
// 111.222
void current_time(unsigned *sec, unsigned *msec) {
	unsigned long cur = current_counter();
	*sec =  (unsigned) (cur / TICKPERSEC); 
	cur -= (*sec) * TICKPERSEC; 
	*msec = (unsigned) (cur / TICKPERMS);	
}

//////////////////////////////
// virtual kernel timers 
struct spinlock timerlock;

struct vtimer {
	TKernelTimerHandler *handler; 
	unsigned long elapseat; 	// sys timer ticks (=us)
	void *param; 
	void *context; 
}; 
static struct vtimer timers[N_TIMERS]; 

// test hw, should fire shortly in the future
__attribute__((unused))
static void sys_timer_test() { 
	unsigned int curVal = get32(TIMER_CLO);
	curVal += interval;
	put32(TIMER_C1, curVal);	
}

void sys_timer_init(void)
{
	initlock(&timerlock, "timer"); 
	memzero(timers, sizeof(timers)); 	// all field zeros	
	// sys_timer_tune_delay(); // can be slow when cache is off 
}

// we have added/removed a virt timer, now adjust the phys timer accordingly
// caller must hold timerlock
// return 0 on success
static int adjust_sys_timer(void)
{
	unsigned long next = (unsigned long)-1; // upcoming firing time, to be determined

	for (int tt = 0; tt < N_TIMERS; tt++) {
		if (!timers[tt].handler)
			continue; 
		if (timers[tt].elapseat < next) {
			if (timers[tt].elapseat < current_counter()) {
				/* timer expired, but handler not called? this could happen on
				qemu when cpu is slow. call the handler here */
				(*timers[tt].handler)(tt, timers[tt].param, timers[tt].context);
				timers[tt].handler = 0; 
			} else 
				/* give "next" a bit slack so current_counter() won't exceed
				"next" before we retuen from this function */
				next = timers[tt].elapseat + 10*1000 /*10ms*/;
		}
	}

	// a known bug (TBD. may occur: when qemu is very slow, or on actual hw
	// timer expired, but handler not called?? should we handle it?
	BUG_ON(current_counter() > next); 

	// if no valid handlers, we leave TIMER_C1 as is. it will trigger a timer
	// irq when wrapping around (~4000 sec later). this is fine as our isr
	// compares 64bit counters. 
	if (next == 0xFFFFFFFFFFFFFFFF) 
		return 0; 

	// the compare reg is only 32 bits so we have to ignore the high 32 bits of
	// the counter. this is ok even if the low 32 bits have to wrap around 
	// in order to match TIMER_C1 (cf the isr)	
	put32(TIMER_C1, (unsigned)next);  

	return 0; 
}

// return: timer id (>=0, <N_TIMERS) allocated. -1 on error
// the clock counter has 64bit, so we assume it won't wrap around
// in the current impl. 
// "handler": callback, to be called in irq context
// NB: caller must hold & then release timerlock
static int ktimer_start_nolock(unsigned delayms, TKernelTimerHandler *handler, 
		void *para, void *context) {
	unsigned t; 
	unsigned long cur; 

	for (t = 0; t < N_TIMERS; t++) {
		if (timers[t].handler == 0) 
			break; 
	}
	if (t == N_TIMERS) {
		E("ktimer_start failed. # max timer reached"); 
		return -1; 
	}

	cur = current_counter(); 
	BUG_ON(cur + TICKPERMS * delayms < cur); // 64bit counter wraps around??

	timers[t].handler = handler; 
	timers[t].param = para; 
	timers[t].context = context; 
	timers[t].elapseat = cur + TICKPERMS * delayms; 

	adjust_sys_timer(); 
	return t; 
}

int ktimer_start(unsigned delayms, TKernelTimerHandler *handler, 
		void *para, void *context) {
	int ret;
	acquire(&timerlock); 
	ret = ktimer_start_nolock(delayms, handler, para, context); 
	release(&timerlock); 
	return ret;
}

// return 0 on okay, -1 if no such timer/handler, 
//	-2 if already fired (will clean anyway)
int ktimer_cancel(int t) {
	unsigned long cur; 

	if (t < 0 || t >= N_TIMERS)
		return -1; 

	cur = current_counter();
	acquire(&timerlock); 

	if (!timers[t].handler) {	// invalid handler
		release(&timerlock); 
		return -1; 
	}

	if (timers[t].elapseat < cur) { // already fired? 
		timers[t].handler = 0; 
		timers[t].context = 0; 
		timers[t].param = 0; 
		release(&timerlock); 
		return -2; 
	}

	timers[t].handler = 0; 

	adjust_sys_timer(); 	
	release(&timerlock);

	return 0;  
}

// the irq handler for sys_timer
// called by irq.c 
void sys_timer_irq(void) 
{
	V("called");	

	// timer1 must have pending match. below could happen under high load. why?
	BUG_ON(!(get32(TIMER_CS) & TIMER_CS_M1));  
	put32(TIMER_CS, TIMER_CS_M1);	// clear timer1 match

	unsigned long cur = current_counter(); 

	acquire(&timerlock); 
	for (int t = 0; t < N_TIMERS; t++) {
		TKernelTimerHandler *h = timers[t].handler; 
		if (h == 0) 
			continue; 
		if (timers[t].elapseat <= cur) { // should fire  
			V("called, id %d h %lx", t, (unsigned long)timers[t].handler);	
			timers[t].handler = 0; 
			(*h)(t, timers[t].param, timers[t].context); 			
		}		
	}
	adjust_sys_timer(); 
	release(&timerlock);
}
#endif 