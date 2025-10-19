#if defined PLAT_RPI3QEMU || defined PLAT_RPI3
#include "plat-rpi3qemu.h"
#else
#error "unimpl"
#endif

// -------------------------- page size constants  ------------------------------ //
// from mmu.h 
#define PAGE_MASK			    0xfffffffffffff000
#define PAGE_SHIFT	 	        12
#define TABLE_SHIFT 		    9
#define SECTION_SHIFT		(PAGE_SHIFT + TABLE_SHIFT)
#define SUPERSECTION_SHIFT      (PAGE_SHIFT + 2*TABLE_SHIFT)      //30, 2^30 = 1GB

#define PAGE_SIZE   		(1 << PAGE_SHIFT)	
#define SECTION_SIZE		(1 << SECTION_SHIFT)	
#define SUPERSECTION_SIZE       (1 << SUPERSECTION_SHIFT)

#define PGROUNDUP(sz)  (((sz)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PAGE_SIZE-1))

// ------------------ phys mem layout ----------------------------------------//
// region reserved for ramdisk. the actual ramdisk can be smaller.
// at this time, uncompressed ramdisk is linked into kernel image and used in place. 
// we therefore don't need additional space
// in the future, compressed ramdisk can be linked, and decompressed into the region below.
// then we can reserve, e.g., 4MB for it
#define RAMDISK_SIZE     0 // (4*1024*1024U)   
#define HIGH_MEMORY     (PHYS_BASE + PHYS_SIZE - RAMDISK_SIZE)
#define MAX_PAGING_PAGES        ((PHYS_SIZE-RAMDISK_SIZE)/PAGE_SIZE)

#ifndef __ASSEMBLER__
	// must be page aligned
	_Static_assert(!(RAMDISK_SIZE & (PAGE_SIZE-1)));
	_Static_assert(!(PHYS_SIZE & (PAGE_SIZE-1)));
#endif 
