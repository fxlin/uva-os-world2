#include "plat.h"
#include "utils.h"
#include "debug.h"
#include "sched.h"

static void handler(TKernelTimerHandle hTimer, void *param, void *context) {
	unsigned sec, msec; 
	current_time(&sec, &msec);
	I("%u.%03u: fired. on cpu %d. htimer %ld, param %lx, contex %lx", sec, msec,
		cpuid(), hTimer, (unsigned long)param, (unsigned long)context); 
}

// to be called in a kernel process
void test_ktimer() {
	unsigned sec, msec; 

	current_time(&sec, &msec); 
	I("%u.%03u start delaying 500ms...", sec, msec); 
	ms_delay(500); 
	current_time(&sec, &msec);
	I("%u.%03u ended delaying 500ms", sec, msec); 

	// start, fire 
	int t = ktimer_start(500, handler, (void *)0xdeadbeef, (void*)0xdeaddeed);
	I("timer start. timer id %u", t); 
	ms_delay(1000);
	I("timer %d should have fired", t); 

	// start two, fire
	t = ktimer_start(500, handler, (void *)0xdeadbeef, (void*)0xdeaddeed);
	I("timer start. timer id %u", t); 
	t = ktimer_start(1000, handler, (void *)0xdeadbeef, (void*)0xdeaddeed);
	I("timer start. timer id %u", t); 
	ms_delay(2000); 
	I("both timers should have fired"); 

	// start, cancel 
	t = ktimer_start(500, handler, (void *)0xdeadbeef, (void*)0xdeaddeed);
	I("timer start. timer id %u", t);
	ms_delay(100); 
	int c = ktimer_cancel(t); 
	I("timer cancel return val = %d", c);
	BUG_ON(c < 0);

	I("there shouldn't be more callback"); 
}

///////////////////
extern void fb_showpicture(void); 
#include "fb.h"

#define PIXELSIZE 4 /*ARGB, expected by /dev/fb*/ 
typedef unsigned int PIXEL; 
#define N 256       // project idea: four color quads has glitches. fix it 

static inline void setpixel(unsigned char *buf, int x, int y, int pit, PIXEL p) {
    assert(x>=0 && y>=0); 
    *(PIXEL *)(buf + y*pit + x*PIXELSIZE) = p; 
}

/* 
    this shows how to flip fb, i.e. change display contents by changing only 
    the "viewport" into the underlying pixel buffers.     

    create a vir fb with four quads, with R/G/B/black. 
    phys (viewport) is of one quad size. 
    then cycle the viewport through the four quads 

    dependency: delay 
    project idea: use virtual timer instead 
        (better efficiency)

    known bug on qemu: some color quads wont dispay correctly.
    ok on rpi3 hw. likely a qemu bug
*/
void test_fb() {
    // fb_showpicture();        // works

    // acquire(&mboxlock);      //it's a test. so no lock

    fb_fini(); 

    the_fb.width = N;
    the_fb.height = N;

    the_fb.vwidth = N*2; 
    the_fb.vheight = N*2; 

    if (fb_init() != 0) BUG();     

    // prefill the fb with four color tiles, once 
    PIXEL b=0x00ff0000, g=0x0000ff00, r=0x000000ff; 
    int x, y;
    int pitch = the_fb.pitch; 
    for (y=0;y<N;y++)
        for (x=0;x<N;x++)
            setpixel(the_fb.fb,x,y,pitch,r); 

    for (y=0;y<N;y++)
        for (x=N;x<2*N;x++)
            setpixel(the_fb.fb,x,y,pitch,(b|r));             

    for (y=N;y<2*N;y++)
        for (x=0;x<N;x++)
            setpixel(the_fb.fb,x,y,pitch,g); 

    for (y=N;y<2*N;y++)
        for (x=N;x<2*N;x++)
            setpixel(the_fb.fb,x,y,pitch,b);             

    // // test --- fill all quads the same color
    // for (y=0;y<2*N;y++)
    //     for (x=0;x<2*N;x++)
    //         setpixel(the_fb.fb,x,y,pitch,b);             

    //what if we dont flush cache?
    __asm_flush_dcache_range(the_fb.fb, the_fb.fb + the_fb.size); 

    while (1) {
        fb_set_voffsets(0,0);
        ms_delay(1500); 
        fb_set_voffsets(0,N);
        ms_delay(1500); 
        fb_set_voffsets(N,0);
        ms_delay(1500); 
        fb_set_voffsets(N,N);
        ms_delay(1500); 
    }
}

////////////////////////////////////////////////
//  two kernel tasks print msgs. 
//  simple test for scheduler and context switch 

// a simple kernel task: print a message, yield
static void kern_task_print(const char *str) {
	printf("Kernel task started at EL %d, pid %d\r\n", get_el(), myproc()->pid);

	while (1) {
		printf("%s", str); 
		ms_delay(10); // NB: spin waiting (silly). for testing sched only
		yield();
	}
}

void test_kern_tasks_print(void) {
	// simple test: two tasks print msgs. 
	int res = copy_process(PF_KTHREAD, (unsigned long)&kern_task_print, 
		(unsigned long) "12345678" /*arg*/,
		"kern-1"); 
	BUG_ON(res<0); 

	res = copy_process(PF_KTHREAD, (unsigned long)&kern_task_print, 
		(unsigned long) "abcdefg" /*arg*/,
		"kern-2"); 
	BUG_ON(res<0);

	// current we are on the "init" task. 
	// if we allow this function to return to kernel_main() which procceeds to wait(), 
	// and our sleep() (called by wait()) is yet to function, the kernel will crash there. so we just keep
	// the init task to keep yielding here forever. 
	while (1)
        	yield();
}

////////////////////////////////////////////////
// test kernel task return, exit() 

// a task returns from its func
static void kern_task_return(const char *str) {
	printf("Kernel task started at EL %d, pid %d\r\n", get_el(), myproc()->pid);
    printf("%s", str); 
    return;     
    // what will happen? 
    // this func is called from ret_from_fork (entry.S). after returning from 
	// this func, it goes back to ret_from_fork and continues there -- in an inf loop
    // (cf entry.S ret_from_fork)
}

// a task calling "exit"
static void kern_task_exit(const char *str) {
	printf("Kernel task started at EL %d, pid %d\r\n", get_el(), myproc()->pid);
    printf("%s", str); 
    exit_process(0); 
}

void test_kern_task_mgmt(void) {
	int res = copy_process(PF_KTHREAD, (unsigned long)&kern_task_return, 
		(unsigned long) "12345678" /*arg*/,
		"kern-1");         
	BUG_ON(res<0); 

	res = copy_process(PF_KTHREAD, (unsigned long)&kern_task_exit, 
		(unsigned long) "abcdefg" /*arg*/,
		"kern-2"); 
	BUG_ON(res<0);    
}

////////////////////////////////////////////////
// test kernel task sleep(), wakeup()
// a toy version of reader/writer pipe
// cf pipe.c 
// quest: "wordsmith"

#define NSIZE 32
static struct spinlock testlock = {.locked=0, .cpu=0, .name="testlock"};
static char pipebuf[NSIZE];
static int nwrite=0, nread=0; 

// write n chars from "str" to pipebuf. block if buf is full
static void do_write(const char *str, int n) {
    int i=0; 
    acquire(&testlock); 
    while (i<n) {
        if (nwrite == nread + NSIZE) { // pipe write full
            /* TODO: your code here */
        } else {
            /* TODO: your code here */
        }
    }
    // done writing n bytes, buf not full, wakeup reader anyway
    /* TODO: your code here */
    release(&testlock); 
}

// read chars from pipebuf to "str". block if buf is empty at the beginning
// char: buf for receiving chars
// n: read at most n chars. 
// return: # of chars actually read
static int do_read(char *str, int n) {
    int i; 

    acquire(&testlock); 
    while (nread == nwrite) {   // pipe empty
        /* TODO: your code here */
    }
    for (i=0; i<n; i++) {
        // pipe empty
            /* TODO: your code here */
        // read out
        /* TODO: your code here */
    }
    /* TODO: your code here */
    release(&testlock); 
    return i; 
}

static void task_writer() {
    // const char *str16="Sky is so clear.";
    // const char *str32="Learning new things expands our minds.";
    // const char *str64="The sunset painted the sky with hues of orange, pink, and gold.";
    
    // while (1) {
    //     do_write(str16, strlen(str16)); // NB: strlen does NOT count '\0'
    //     ms_delay(100); // spin waiting (silly). for testing only
    //     do_write(str32, strlen(str32));
    //     ms_delay(100); // spin waiting (silly). for testing only
    //     do_write(str64, strlen(str64));
    //     ms_delay(100); // spin waiting (silly). for testing only
    // }

    // around 256 bytes    
    static const char wordsworth[] = "O Nature! Thou art ever kind and free,"
                    "Thy gentle whispers calm the restless soul;"
                    "The streams, the woods, the sky, the endless sea,"
                    "In thee, we find our being's truest goal.";

    while (1) {
        do_write(wordsworth, strlen(wordsworth)); // NB: strlen does NOT count '\0'
        ms_delay(100); // spin waiting (silly). for testing only
    }
}

static void task_reader() {
#define MYBUFLEN 17 // can be small 
    char mybuf[MYBUFLEN];
    int n; 
    while (1) {
        n = do_read(mybuf, MYBUFLEN-1); // need one char for '\0'
        mybuf[n] = '\0';
        W("read: %d bytes. %s", n, mybuf);
    }
}

void test_kern_reader_writer() {
	int res = copy_process(PF_KTHREAD, (unsigned long)&task_writer, 
		0 /*arg*/, "writer");         
	BUG_ON(res<0); 

	res = copy_process(PF_KTHREAD, (unsigned long)&task_reader, 
		0 /*arg*/, "reader"); 
	BUG_ON(res<0);    
}

////////////////////////////////////////////////
//  N kernel tasks drawing N donuts
//  stress test for scheduler and context switch (also more eye candy)
//  create N tasks, each drawing a donut on a canvas region (with idx)
//  each task: draw donut animation on a canvas region. maximum 4 regions
//  modeled after test_kern_tasks_print()

// quest: "two donuts"

extern void donut(int idx); 	//donut.c
extern void donut_canvas_init(void); //donut.c don't forget to init canvas -- once
void kern_task_donut(int idx) {
	printf("process started EL %d, pid %d idx %d\r\n", 
        get_el(), myproc()->pid, idx);
    // exp: diff proirities --> donuts will turn at diff rates
	/* TODO: your code here */
}

void test_kern_tasks_donut(void) {
    char name[10]; 
    int res; 

    donut_canvas_init(); 
    
    // spawn N donut tasks 
    for (int i=0; i<N_DONUTS; i++) {
        snprintf(name, 10, "donut-%d", i); 
        /* TODO: your code here */
    }

	// current we are on the "init" task. 
	// if we allow this function to return to kernel_main() which procceeds to wait(), 
	// and our sleep() (called by wait()) is yet to function, the kernel will crash there. so we just keep
	// the init task to keep yielding here forever. 	
	while (1)
        	yield();
	
    // some ideas to demonstrate scheduling:
    // give high priority to some tasks, so their donuts turn faster
    // schedule timeslice - make all donuts turn in sync (virtually)
    //      less visible qemu (b/c it runs fast), more visible on rpi3 (w/o cache, slow)
    // qemu -- make all donuts turn at same time (virtually), 
    //      turn at diff rates 
    //      -- a donut skips certain % of frames (yield multiple times)
}


