#ifndef	_UTILS_H
#define	_UTILS_H

#include <stdint.h>
#include "param.h"

#include "printf.h"
struct spinlock; 
struct task_struct; 
struct fb_struct; 

// ------------------- utils.S ----------------------------- //
extern void delay (unsigned long cycles);   

#define put32(addr, v) \
    do \
    {*(volatile unsigned int *)(addr) = v;}    \
    while (0)

#define get32(addr)  (*(volatile unsigned int *)(addr))
extern int get_el ( void );
void panic(char *s);

// ------------------- uart ----------------------------- //
void uart_init (void);
char uart_recv ( void );
void putc ( void* p, char c );
// new APIs from xv6
void            uartintr(void);
void            uartputc(int);
void            uartputc_sync(int);
int             uartgetc(void);

// ------------------- timer ----------------------------- //
/* These are for "System Timer". See timer.c for details */
void sys_timer_init ( void );
void sys_timer_irq ( void );

// both busy spinning
void ms_delay(unsigned ms); 
void us_delay(unsigned us);

void current_time(unsigned *sec, unsigned *msec);

// kernel timers w/ callbacks, atop sys timer
typedef unsigned long TKernelTimerHandle;	// =idx in kernel table for timers 
typedef void TKernelTimerHandler (TKernelTimerHandle hTimer, void *pParam, void *pContext);

int ktimer_start(unsigned delayms, TKernelTimerHandler *handler, 
		void *para, void *context); 
int ktimer_cancel(int timerid);

/* below are for Arm generic timers */
void generic_timer_init ( void );
void handle_generic_timer_irq ( void );

extern unsigned int ticks; 

// ------------------- spinlock ---------------------------- //
void            acquire(struct spinlock*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            push_off(void);
void            pop_off(void);

// ------------------- sched ---------------------------- //
void exit_process(int);
void sleep(void *, struct spinlock *);
int wait(uint64_t);
int wakeup(void *);

// ------------------- irq ---------------------------- //
void enable_interrupt_controller(int coreid); // irq.c 

// utils.S
void irq_vector_init( void );    
void enable_irq( void ); 
void disable_irq( void );
int is_irq_masked(void); 
/*return 1 if irq enabled, 0 otherwise*/
static inline int intr_get(void) {return 1-is_irq_masked();}; 
int cpuid(void);  // util.S must be called with irq disabled

// alloc.c 
unsigned int paging_init();
unsigned long get_free_page();      // pa
void free_page(unsigned long p);    // pa 
int reserve_phys_region(unsigned long pa_start, unsigned long size); 
int free_phys_region(unsigned long pa_start, unsigned long size); 

// ----------------  mm.c ---------------------- //
//src/n must be 8 bytes aligned   util.S
void memzero_aligned(void *src, unsigned long n);  
//dst/src/n must be 8 bytes aligned    util.S
void* memcpy_aligned(void* dst, const void* src, unsigned int n);

// string.c
int memcmp(const void *, const void *, uint);
void *memmove(void *, const void *, uint);
void *memcpy(void *, const void *, uint);
void *memset(void *, int, uint);
void memzero(void *, uint);
char *safestrcpy(char *, const char *, int);
int strlen(const char *);
int strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);
int atoi(const char *s);

// sched.c
extern void sched_init(void);
extern void yield(void);
extern void schedule(void);
extern void timer_tick(void);
extern void preempt_disable(void);
extern void preempt_enable(void);
extern void switch_to(struct task_struct* next);
extern void cpu_switch_to(struct task_struct* prev, struct task_struct* next);	// switch.S
void procdump(void); 

struct task_struct *myproc(void); 

int copy_process(unsigned long clone_flags, unsigned long fn, 
    unsigned long arg, const char *name);

// mbox.c
#define MAC_SIZE        6   // in bytes
int get_mac_addr(unsigned char buf[MAC_SIZE]);
int set_powerstate(unsigned deviceid, int on); 
int get_board_serial(unsigned long *s);

int fb_init(void); 
int fb_fini(void); 
int fb_set_voffsets(int offsetx, int offsety);

/*      useful macros       */
// linux
#define likely(exp)     __builtin_expect (!!(exp), 1)
#define unlikely(exp)   __builtin_expect (!!(exp), 0)

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)((char *)__mptr - __builtin_offsetof(type,member));})

static inline void warn_failed (const char *pExpr, const char *pFile, unsigned nLine) {
    printf("warning: %s at %s:%u\n", pExpr, pFile, nLine); 
}

#define assert(expr)    (likely (expr)        \
                            ? ((void) 0)           \
                            : assertion_failed (#expr, __FILE__, __LINE__))
#define BUG_ON(exp)	assert (!(exp))
#define BUG()		assert (0)

#define WARN_ON(expr)    (likely (!(expr))        \
                            ? ((void) 0)           \
                            : warn_failed (#expr, __FILE__, __LINE__))

#define MAX(a,b) ((a) > (b) ? a : b)
#define MIN(a,b) ((a) < (b) ? a : b)
#define ABS(x) ((x) < 0 ? -(x) : (x))

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#include "debug.h"

// from mmu.h 
#define PGROUNDUP(sz)  (((sz)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PAGE_SIZE-1))

// cache ops, util.S
void __asm_invalidate_dcache_range(void* start_addr, void* end_addr);
void __asm_flush_dcache_range(void* start_addr, void* end_addr);

#endif  /*_UTILS_H */
