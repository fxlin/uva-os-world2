// ----------------- irq --------------------------- //

#if 0
.globl irq_vector_init
irq_vector_init:
	adr	x0, vectors		// load VBAR_EL1 with virtual
	msr	vbar_el1, x0		// vector table address
	ret
#endif

// daifclr/set 
.globl enable_irq
enable_irq:
	msr    daifclr, #0b0010 
	ret

.globl disable_irq
disable_irq:
	msr	daifset, #0b0010 
	ret 

.global is_irq_masked
is_irq_masked:
	# fxl: whereas daifset/clr are lowest four bits, daif seems bit9--6
	# https://developer.arm.com/documentation/ddi0601/2023-12/AArch64-Registers/DAIF--Interrupt-Mask-Bits
	mrs x0, daif 
	lsr x0, x0, #7 
	and x0, x0, #1
	ret

.global cpuid
cpuid: 
	mrs	x0, mpidr_el1
	and	x0, x0, #0xFF
	ret

// ----------------------- pgd --------------------------------------------//
// ttbr0, user va
.globl set_pgd
set_pgd:
	msr	ttbr0_el1, x0
	tlbi vmalle1is
  	DSB ISH              // ensure completion of TLB invalidation
	isb
	ret

.globl get_pgd
get_pgd:
	mov x1, 0
	ldr x0, [x1]
	mov x0, 0x1000
	msr	ttbr0_el1, x0
	ldr x0, [x1]
	ret

// ---------------------- misc -------------------------------------------- //
// the _aligned funcs are faster than normal variants, but MUST BE used with 
// care (unaligned addr will corrupt/miss contents) to avoid nasty bugs. 
// unless the buf is large, the extra speed is not worth it
.globl memcpy_aligned
memcpy_aligned:
 	ldr x3, [x1], #8
 	str x3, [x0], #8
	subs x2, x2, #8
 	b.gt memcpy_aligned
 	ret

.globl memzero_aligned
memzero_aligned:
	str xzr, [x0], #8
	subs x1, x1, #8
	b.gt memzero_aligned
	ret

.globl get_el
get_el:
	mrs x0, CurrentEL
	lsr x0, x0, #2
	ret

.globl put32
put32:
	str w1,[x0]
	ret

.globl get32
get32:
	ldr w0,[x0]
	ret

.globl delay
delay:
	subs x0, x0, #1
	bne delay
	ret

// aarch64 cache ops, from Rt thread OS
// https://raw.githubusercontent.com/RT-Thread/rt-thread/master/libcpu/aarch64/common/cache.S

/*
 * void __asm_flush_dcache_range(start, end)
 *
 * clean & invalidate data cache in the range
 *
 * x0: start address
 * x1: end address
  * (fxl: both VA, "dc civac" - dc cache and/or inv by VA)
 */
.globl __asm_flush_dcache_range
__asm_flush_dcache_range:
    mrs    x3, ctr_el0
    lsr    x3, x3, #16
    and    x3, x3, #0xf
    mov    x2, #4
    lsl    x2, x2, x3        /* cache line size */

    /* x2 <- minimal cache line size in cache system */
    sub    x3, x2, #1
    bic    x0, x0, x3

1:  dc    civac, x0    /* clean & invalidate data or unified cache */
    add    x0, x0, x2
    cmp    x0, x1
    b.lo    1b
    dsb    sy
    ret

/* void __asm_invalidate_dcache_range(start, end)
 *
 * invalidate data cache in the range
 *
 * x0: start address
 * x1: end address
 */
.globl __asm_invalidate_dcache_range
__asm_invalidate_dcache_range:
    mrs    x3, ctr_el0
    lsr    x3, x3, #16
    and    x3, x3, #0xf
    mov    x2, #4
    lsl    x2, x2, x3        /* cache line size */

    /* x2 <- minimal cache line size in cache system */
    sub    x3, x2, #1
    bic    x0, x0, x3

1:  dc   ivac, x0    /* invalidate data or unified cache */
    add    x0, x0, x2
    cmp    x0, x1
    b.lo    1b
    dsb    sy
    ret

// from Rt thread OS
// https://raw.githubusercontent.com/RT-Thread/rt-thread/master/libcpu/aarch64/common/cache.S
/*
 * void __asm_dcache_level(level)
 *
 * flush or invalidate one level cache.
 *
 * x0: cache level
 * x1: 0 clean & invalidate, 1 invalidate only
 * x2~x9: clobbered
 */
.globl __asm_dcache_level
__asm_dcache_level:
    lsl    x12, x0, #1
    msr    csselr_el1, x12        /* select cache level */
    isb                /* sync change of cssidr_el1 */
    mrs    x6, ccsidr_el1        /* read the new cssidr_el1 */
    and    x2, x6, #7        /* x2 <- log2(cache line size)-4 */
    add    x2, x2, #4        /* x2 <- log2(cache line size) */
    mov    x3, #0x3ff
    and    x3, x3, x6, lsr #3    /* x3 <- max number of #ways */
    clz    w5, w3            /* bit position of #ways */
    mov    x4, #0x7fff
    and    x4, x4, x6, lsr #13    /* x4 <- max number of #sets */
    /* x12 <- cache level << 1 */
    /* x2 <- line length offset */
    /* x3 <- number of cache ways - 1 */
    /* x4 <- number of cache sets - 1 */
    /* x5 <- bit position of #ways */

loop_set:
    mov    x6, x3            /* x6 <- working copy of #ways */
loop_way:
    lsl    x7, x6, x5
    orr    x9, x12, x7        /* map way and level to cisw value */
    lsl    x7, x4, x2
    orr    x9, x9, x7        /* map set number to cisw value */
    tbz    w1, #0, 1f
    dc    isw, x9
    b    2f
1:    dc    cisw, x9        /* clean & invalidate by set/way */
2:    subs    x6, x6, #1        /* decrement the way */
    b.ge    loop_way
    subs    x4, x4, #1        /* decrement the set */
    b.ge    loop_set

    ret

/*
 * void __asm_flush_dcache_all(int invalidate_only)
 *
 * x0: 0 clean & invalidate, 1 invalidate only
 *
 * flush or invalidate all data cache by SET/WAY.
 */
.globl __asm_dcache_all
__asm_dcache_all:
    mov    x1, x0
    dsb    sy
    mrs    x10, clidr_el1        /* read clidr_el1 */
    lsr    x11, x10, #24
    and    x11, x11, #0x7        /* x11 <- loc */
    cbz    x11, finished        /* if loc is 0, exit */
    mov    x15, lr
    mov    x0, #0            /* start flush at cache level 0 */
    /* x0  <- cache level */
    /* x10 <- clidr_el1 */
    /* x11 <- loc */
    /* x15 <- return address */

loop_level:
    lsl    x12, x0, #1
    add    x12, x12, x0        /* x0 <- tripled cache level */
    lsr    x12, x10, x12
    and    x12, x12, #7        /* x12 <- cache type */
    cmp    x12, #2
    b.lt    skip            /* skip if no cache or icache */
    bl    __asm_dcache_level    /* x1 = 0 flush, 1 invalidate */
skip:
    add    x0, x0, #1        /* increment cache level */
    cmp    x11, x0
    b.gt    loop_level

    mov    x0, #0
    msr    csselr_el1, x0        /* restore csselr_el1 */
    dsb    sy
    isb
    mov    lr, x15

finished:
    ret

.globl __asm_flush_dcache_all
__asm_flush_dcache_all:
    mov    x0, #0
    b    __asm_dcache_all

.globl __asm_invalidate_dcache_all
__asm_invalidate_dcache_all:
    mov    x0, #0x1
    b    __asm_dcache_all
	
//////////// uart.S 
// useful for secondary cores early debugging, before they have valid sp
// but after core0 has done uart_init()
// void uart_send_pa(char c); 

#define VA_START 			0xffff000000000000
#define PBASE   0x3F000000
#define AUX_MU_LSR_REG  (PBASE+0x00215054)
#define AUX_MU_IO_REG   (PBASE+0x00215040)

.globl uart_send_pa
uart_send_pa:
1:
	ldr x1, =AUX_MU_LSR_REG
	ldr w2, [x1]
	and w2, w2, #0x20
	cbz w2, 1b
	ldr x1, =AUX_MU_IO_REG
	str w0, [x1]
	ret 

// NB: must write 32, not 64
.globl uart_send_va
uart_send_va:
1:
	ldr x4, =VA_START
	ldr x1, =AUX_MU_LSR_REG
	ldr w2, [x1, x4]
	and w2, w2, #0x20
	cbz w2, 1b
	ldr x1, =AUX_MU_IO_REG
	str w0, [x1, x4]
	ret 