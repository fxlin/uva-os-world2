// included for both plat-rpi3, plat-rpi3qemu

#ifndef _PLAT_RPI3QEMU_H
#define _PLAT_RPI3QEMU_H

#define RASPPI 3            // for defs copied from circle
#ifndef AARCH
    #define AARCH 64
#endif

// QEMU's rpi3 emulation, for qemu's "-M raspi3"
// will be included by both .S and .c
// ---------------- memory layout --------------------------- //
// cf qemu monitor "info mtree"
#define DEVICE_BASE     0x3f000000
#define DEVICE_LOW      0x40000000 
#define DEVICE_HIGH     0x40000100

#define PHYS_BASE       0x00000000              // start of sys mem
#define KERNEL_START    0x00080000              // qemu will load ker to this addr and boot
#define PHYS_SIZE       0x3f000000    


// from circle include/circle/bcm2835.h
#if RASPPI == 1
#define ARM_IO_BASE		0x20000000
#elif RASPPI <= 3
#define ARM_IO_BASE		0x3F000000
#else
#define ARM_IO_BASE		0xFE000000
#endif
#define ARM_IO_END		(ARM_IO_BASE + 0xFFFFFF)

#define GPU_IO_BASE		0x7E000000

#define GPU_CACHED_BASE		0x40000000
#define GPU_UNCACHED_BASE	0xC0000000

#if RASPPI == 1
	#ifdef GPU_L2_CACHE_ENABLED
		#define GPU_MEM_BASE	GPU_CACHED_BASE
	#else
		#define GPU_MEM_BASE	GPU_UNCACHED_BASE
	#endif
#else
	#define GPU_MEM_BASE	GPU_UNCACHED_BASE
#endif

// Convert ARM address to GPU bus address (does also work for aliases)
#define BUS_ADDRESS(addr)	(((addr) & ~0xC0000000) | GPU_MEM_BASE)

//
// General Purpose I/O
//
#define ARM_GPIO_BASE		(ARM_IO_BASE + 0x200000)

#define ARM_GPIO_GPFSEL0	(ARM_GPIO_BASE + 0x00)
#define ARM_GPIO_GPFSEL1	(ARM_GPIO_BASE + 0x04)
#define ARM_GPIO_GPFSEL4	(ARM_GPIO_BASE + 0x10)
#define ARM_GPIO_GPSET0		(ARM_GPIO_BASE + 0x1C)
#define ARM_GPIO_GPCLR0		(ARM_GPIO_BASE + 0x28)
#define ARM_GPIO_GPLEV0		(ARM_GPIO_BASE + 0x34)
#define ARM_GPIO_GPEDS0		(ARM_GPIO_BASE + 0x40)
#define ARM_GPIO_GPREN0		(ARM_GPIO_BASE + 0x4C) // GPIO Pin Rising Edge Detect Enable
#define ARM_GPIO_GPFEN0		(ARM_GPIO_BASE + 0x58) // GPIO Pin Falling Edge Detect Enable
#define ARM_GPIO_GPHEN0		(ARM_GPIO_BASE + 0x64) // GPIO Pin High Detect Enable
#define ARM_GPIO_GPLEN0		(ARM_GPIO_BASE + 0x70)  // GPIO Pin Low Detect Enable
#define ARM_GPIO_GPAREN0	(ARM_GPIO_BASE + 0x7C)
#define ARM_GPIO_GPAFEN0	(ARM_GPIO_BASE + 0x88)
#if RASPPI <= 3
#define ARM_GPIO_GPPUD		(ARM_GPIO_BASE + 0x94)
#define ARM_GPIO_GPPUDCLK0	(ARM_GPIO_BASE + 0x98)
#else
#define ARM_GPIO_GPPINMUXSD	(ARM_GPIO_BASE + 0xD0)
#define ARM_GPIO_GPPUPPDN0	(ARM_GPIO_BASE + 0xE4)
#define ARM_GPIO_GPPUPPDN1	(ARM_GPIO_BASE + 0xE8)
#define ARM_GPIO_GPPUPPDN2	(ARM_GPIO_BASE + 0xEC)
#define ARM_GPIO_GPPUPPDN3	(ARM_GPIO_BASE + 0xF0)
#endif

// ---------------- irq ------------------------------------ //
// "BCM2837 ARM Peripherals Revised V2-1. pp.112 Sec 7.5 registers"
// cf Circle bcm2835int.h
#define PBASE           0x3F000000UL      // start of peripheral addr
#define IRQ_BASIC_PENDING	(PBASE+0x0000B200)  // gpu's own irqs + common irqs
    // bit 2: doorbell 0 (irq #66)
#define IRQ_PENDING_1		(PBASE+0x0000B204)  // irq 0..31 from gpu side
    #define SYSTEM_TIMER_IRQ_0	(1 << 0)
    #define SYSTEM_TIMER_IRQ_1	(1 << 1)
    #define SYSTEM_TIMER_IRQ_2	(1 << 2)
    #define SYSTEM_TIMER_IRQ_3	(1 << 3)
    #define IRQ_PENDING_1_USB (1<<9)
    #define IRQ_PENDING_1_DMA(x) (1<<(16+x))      // dma channel 0..12
    #define IRQ_PENDING_1_AUX (1<<29)       // aux(mini uart)
#define IRQ_PENDING_2		(PBASE+0x0000B208)  // irq 32..63 from gpu side
#define FIQ_CONTROL		    (PBASE+0x0000B20C)
#define ENABLE_IRQS_1		(PBASE+0x0000B210)
    #define ENABLE_IRQS_1_USB (1<<9)
    #define ENABLE_IRQS_1_DMA(x) (1<<(16+x))      // dma channel 0..12
    #define ENABLE_IRQS_1_AUX (1<<29)
#define ENABLE_IRQS_2		(PBASE+0x0000B214)
#define ENABLE_BASE_IRQS	(PBASE+0x0000B218) // "Base Interrupt enable register"
#define DISABLE_IRQS_1		(PBASE+0x0000B21C)
#define DISABLE_IRQS_2		(PBASE+0x0000B220)
#define DISABLE_BASIC_IRQS	(PBASE+0x0000B224)


// See BCM2836 ARM-local peripherals at
// https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf
#define LPBASE              0x40000000UL
#define TIMER_INT_CTRL_0    0x40000040UL    // per core irq control 
#define INT_SOURCE_0        (LPBASE+0x60)   // "CORE0_IRQ_SOURCE" in the manual above

#define TIMER_INT_CTRL_0_VALUE  (1 << 1) 
#define GENERIC_TIMER_INTERRUPT (1U<<1) // CNTPNSIRQ
#define GPU_SIDE_INTERRUPT      (1U<<8)        // GPU side interrupt, "Interrupt source bits" in manual above

// ---------------- mbox  ------------------------------------ //
// Copyright (C) 2018 bzt (bztsrc@github) (cf. CREDITS)

#ifndef __ASSEMBLER__
/* a properly aligned buffer */
extern volatile unsigned int mbox[36];
int mbox_call(unsigned char ch);
#endif

#define MBOX_REQUEST    0

/* channels */
#define MBOX_CH_POWER   0
#define MBOX_CH_FB      1
#define MBOX_CH_VUART   2
#define MBOX_CH_VCHIQ   3
#define MBOX_CH_LEDS    4
#define MBOX_CH_BTNS    5
#define MBOX_CH_TOUCH   6
#define MBOX_CH_COUNT   7
#define MBOX_CH_PROP    8

/* tags */
#define MBOX_TAG_SETPOWER       0x28001
#define MBOX_TAG_SETCLKRATE     0x38002
#define MBOX_TAG_LAST           0

// ---------------- system timer ------------------------------------ //
#define TIMER_CS        (PBASE+0x00003000)
#define TIMER_CLO       (PBASE+0x00003004)      // counter low 32bit, ro
#define TIMER_CHI       (PBASE+0x00003008)      // counter high 32bit, ro 
#define TIMER_C0        (PBASE+0x0000300C)
#define TIMER_C1        (PBASE+0x00003010)
#define TIMER_C2        (PBASE+0x00003014)
#define TIMER_C3        (PBASE+0x00003018)

#define TIMER_CS_M0	(1 << 0)
#define TIMER_CS_M1	(1 << 1)
#define TIMER_CS_M2	(1 << 2)
#define TIMER_CS_M3	(1 << 3)

// --------------- mini uart ------------------------------- //
#define UART_PHYS 0x3F000000    // cf mini_uart.c

//
// Platform DMA Controller
//
#define ARM_DMA_BASE		(ARM_IO_BASE + 0x7000)

//
// Pulse Width Modulator
//
#define ARM_PWM_BASE		(ARM_IO_BASE + 0x20C000)

#define ARM_PWM_CTL		(ARM_PWM_BASE + 0x00)
#define ARM_PWM_STA		(ARM_PWM_BASE + 0x04)
#define ARM_PWM_DMAC		(ARM_PWM_BASE + 0x08)
#define ARM_PWM_RNG1		(ARM_PWM_BASE + 0x10)
#define ARM_PWM_DAT1		(ARM_PWM_BASE + 0x14)
#define ARM_PWM_FIF1		(ARM_PWM_BASE + 0x18)
#define ARM_PWM_RNG2		(ARM_PWM_BASE + 0x20)
#define ARM_PWM_DAT2		(ARM_PWM_BASE + 0x24)

#if RASPPI >= 4
#define ARM_PWM1_BASE		(ARM_IO_BASE + 0x20C800)

#define ARM_PWM1_CTL		(ARM_PWM1_BASE + 0x00)
#define ARM_PWM1_STA		(ARM_PWM1_BASE + 0x04)
#define ARM_PWM1_DMAC		(ARM_PWM1_BASE + 0x08)
#define ARM_PWM1_RNG1		(ARM_PWM1_BASE + 0x10)
#define ARM_PWM1_DAT1		(ARM_PWM1_BASE + 0x14)
#define ARM_PWM1_FIF1		(ARM_PWM1_BASE + 0x18)
#define ARM_PWM1_RNG2		(ARM_PWM1_BASE + 0x20)
#define ARM_PWM1_DAT2		(ARM_PWM1_BASE + 0x24)
#endif

//
// Clock Manager			
// Soc manual: "Clock Manager General Purpose Clocks Control"
#define ARM_CM_BASE		(ARM_IO_BASE + 0x101000)

#define ARM_CM_GP0CTL		(ARM_CM_BASE + 0x70)
    // GP1CTL +0x78     GP2CTL +0x80    (fxl: 8 bytes apart, interleaved with regs below
#define ARM_CM_GP0DIV		(ARM_CM_BASE + 0x74)
    // see above. 
#define ARM_CM_PASSWD 		(0x5A << 24)




#endif