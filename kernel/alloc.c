/* simple memory allocation. in fact lab2 not uses page allocator (only phys
    region reservation); so this can be further simplified.*/

// #define K2_DEBUG_VERBOSE
// #define K2_DEBUG_INFO
#define K2_DEBUG_WARN

#include "plat.h"
#include "utils.h"
#include "spinlock.h"


/* Phys memory layout:	cf paging_init() below. 

	Upon init, the Rpi3 hw (GPU) will allocate framebuffer at certain addr. We
	need to know that addr in order to decide paging memory usable to the
	kernel. However, we won't know that addr until we initialize the GPU (cf
	mbox.c). Based on my observation, GPU always allocates the framebuffer at
	fixed locations ("GUESS_FB_BASE_PA" below) near the end of phys mem. 

	Based on the observation, we exclude GUESS_FB_BASE_PA--HIGH_MEMORY from the
	paging memory. (Hope we don't waste too much phys mem)

	If later the GPU init code (mbox.c) finds these assumption is wrong, 
	mem reservation will fail and kernel will panic. 
*/

/* qemu always set to 0x3c100000 (a safe choice, I suppose?); the rpi3 hw will
	change based on the requested fb size, e.g. 0x3e8fa000 for 1024x768;
	0x3e7fe000 for 1360x768... so pick "one size fits all" */
#define GUESS_FB_BASE_PA 0x3c100000
#define HIGH_MEMORY0 GUESS_FB_BASE_PA // the actual "highmem"

/* "mem_map" are flags covering from LOW_MEMORY to HIGH_MEMORY, including: 
(1) region for page allocator 
(2) the possible framebuffer region (to be reserved).
(3) the malloc region (to be reserved).
one byte for a page. 1=allocated */
static unsigned char mem_map [ MAX_PAGING_PAGES ] = {0,}; 
unsigned paging_pages_used = 0, paging_pages_total = 0;

/*  Minimalist page allocation 
	all alloc/free funcs below are locked (SMP safe) */
struct spinlock alloc_lock = {.locked=0, .cpu=0, .name="alloc_lock"}; 

static unsigned long LOW_MEMORY = 0; 	// pa
static unsigned long PAGING_PAGES = 0; 
extern char kernel_end; // linker.ld

/* allocate a page (zero filled). return pa of the page. 0 if failed */
unsigned long get_free_page() {
	acquire(&alloc_lock);
	for (int i = 0; i < PAGING_PAGES-MALLOC_PAGES; i++){
		if (mem_map[i] == 0){
			mem_map[i] = 1; paging_pages_used++;
			release(&alloc_lock);
			unsigned long page = LOW_MEMORY + i*PAGE_SIZE;
			memzero_aligned((void *)page, PAGE_SIZE);
			return page;
		}
	}
	release(&alloc_lock);
	return 0;
}

/* free a page. @p is pa of the page. */
void free_page(unsigned long p){
	acquire(&alloc_lock);
	mem_map[(p - LOW_MEMORY)>>PAGE_SHIFT] = 0; paging_pages_used--;
	release(&alloc_lock);
}

/* reserve a phys region. all pages must be unused previously. 
	caller MUST hold alloc_lock
	is_reserve: 1 for reserve, 0 for free
	return 0 if OK  */
static int _reserve_phys_region(unsigned long pa_start, 
	unsigned long size, int is_reserve) {
	if ((pa_start & ~PAGE_MASK) != 0 || (size & ~PAGE_MASK) != 0) // must align
		{W("pa_start %lx size %lx", pa_start, size);BUG(); return -1;}

	for (unsigned i = ((pa_start-LOW_MEMORY)>>PAGE_SHIFT); 
			i<((pa_start-LOW_MEMORY+size)>>PAGE_SHIFT); i++){
		if (mem_map[i] == is_reserve)	
			{return -2;}      // page already reserved / freed? 
	}	
	for (unsigned i = ((pa_start-LOW_MEMORY)>>PAGE_SHIFT); 
		i<((pa_start-LOW_MEMORY+size)>>PAGE_SHIFT); i++){
		mem_map[i] = is_reserve; 
	}
	if (is_reserve) paging_pages_used += (size>>PAGE_SHIFT); 
		else paging_pages_used -= (size>>PAGE_SHIFT);

	I("%s: %s. pa_start %lx -- %lx size %lx",
		 __func__, is_reserve?"reserved":"freed", 
		 pa_start, pa_start+size, size);
	return 0; 
}

/* same as above. but caller MUST NOT hold alloc_lock */
int reserve_phys_region(unsigned long pa_start, unsigned long size) {
	int ret; 
	acquire(&alloc_lock); 
	ret = _reserve_phys_region(pa_start, size, 1/*reserve*/);
	release(&alloc_lock); 
	return ret; 
}

/* same as above. but caller MUST NOT hold alloc_lock */
int free_phys_region(unsigned long pa_start, unsigned long size) {
	int ret; 
	acquire(&alloc_lock); 
	ret = _reserve_phys_region(pa_start, size, 0/*free*/);
	release(&alloc_lock); 
	return ret; 
}

/* init kernel's memory mgmt 
	return: # of paging pages */
unsigned int paging_init() {
	LOW_MEMORY = PGROUNDUP((unsigned long)&kernel_end);
	PAGING_PAGES = (HIGH_MEMORY0 - LOW_MEMORY) / PAGE_SIZE; // comment above
	
    BUG_ON(2 * MALLOC_PAGES >= PAGING_PAGES); // too many malloc pages 

    /* reserve a virtually contig region for malloc()  */
    if (MALLOC_PAGES) {
        acquire(&alloc_lock); 
		int ret = _reserve_phys_region(HIGH_MEMORY0-MALLOC_PAGES*PAGE_SIZE, 
			MALLOC_PAGES*PAGE_SIZE, 1); 
        BUG_ON(ret); 
        release(&alloc_lock);
    }

	printf("phys mem: %08x -- %08x\n", PHYS_BASE, PHYS_BASE + PHYS_SIZE);
	printf("\t kernel: %08x -- %08lx\n", KERNEL_START, (unsigned long)(&kernel_end));
	printf("\t paging mem: %08lx -- %08x\n", LOW_MEMORY, HIGH_MEMORY0-(MALLOC_PAGES<<PAGE_SHIFT));
	printf("\t\t %lu%s %ld pages\n", 
		int_val((HIGH_MEMORY0 - LOW_MEMORY)),
		int_postfix((HIGH_MEMORY0 - LOW_MEMORY)),
		PAGING_PAGES);
    printf("\t malloc mem: %08x -- %08x\n", HIGH_MEMORY0-(MALLOC_PAGES<<PAGE_SHIFT), HIGH_MEMORY0);
	printf("\t\t %lu%s\n", int_val(MALLOC_PAGES * PAGE_SIZE),
                                 int_postfix(MALLOC_PAGES * PAGE_SIZE)); 
	printf("\t reserved for framebuffer: %08x -- %08x\n", 
		HIGH_MEMORY0, HIGH_MEMORY);

	paging_pages_total = ((HIGH_MEMORY0-LOW_MEMORY)>>PAGE_SHIFT) - MALLOC_PAGES; 

	return PAGING_PAGES; 
}
