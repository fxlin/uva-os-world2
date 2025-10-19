#ifndef _SYSREGS_H
#define _SYSREGS_H

// ***************************************
// SCTLR_EL1, System Control Register (EL1), Page 2654 of AArch64-Reference-Manual.
// or Google "SCTLR_EL1"
// ***************************************

#define SCTLR_RESERVED                  (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11)   // mandatory reserved bits, cf the manual
#define SCTLR_EE_LITTLE_ENDIAN          (0 << 25)
#define SCTLR_EOE_LITTLE_ENDIAN         (0 << 24)
#define SCTLR_I_CACHE_DISABLED          (0 << 12)
#define SCTLR_D_CACHE_DISABLED          (0 << 2)
#define SCTLR_MMU_DISABLED              (0 << 0)

#define SCTLR_VALUE_MMU_DISABLED	(SCTLR_RESERVED | SCTLR_EE_LITTLE_ENDIAN |\
                                     SCTLR_I_CACHE_DISABLED | SCTLR_D_CACHE_DISABLED |\
                                      SCTLR_MMU_DISABLED)

// cf: https://raw.githubusercontent.com/LdB-ECM/Raspberry-Pi/master/10_virtualmemory/SmartStart64.S
// also checked against the manual above 
// fxl: if bit A is enabled, user program will trigger exception (misaligned access??)
#define SCTLR_VALUE     (SCTLR_RESERVED |  /* mandatory reserved bits, cf the manual */\
					  (1 << 12)  |      /* I, Instruction cache enable. This is an enable bit for instruction caches at EL0 and EL1 */\
					  (1 << 3)   |		/* SA, Stack Alignment Check Enable */\
					  (1 << 4)   |		/* SA0, Stack Alignment Check Enable for EL0 */\
					  (1 << 2)   |		/* C, Data cache enable. This is an enable bit for data caches at EL0 and EL1 */\
					  (1 << 0) )		/* set M, enable MMU */


// below: gcc by default does not do aligned access; will have to change lots of makefiles
					//   (1 << 1)   |		/* A, Alignment check enable bit */\


// MMU on but no caching (for testing)
#define SCTLR_VALUE_UNCACHED     (SCTLR_RESERVED |  /* mandatory reserved bits, cf the manual */\
					  (1 << 4)   |		/* SA0, Stack Alignment Check Enable for EL0 */\
					  (1 << 3)   |		/* SA, Stack Alignment Check Enable */\
					  (1 << 1)   |		/* A, Alignment check enable bit */\
					  (1 << 0) )		/* set M, enable MMU */

// ***************************************
// HCR_EL2, Hypervisor Configuration Register (EL2), Page 2487 of AArch64-Reference-Manual.
// ***************************************
// HCR_RW: "The Execution state for EL1 is AArch64. The Execution state for EL0 is determined by the 
//          current value of PSTATE.nRW when executing at EL0"
#define HCR_RW	    			(1 << 31)
#define HCR_VALUE			    HCR_RW

// ***************************************
// SCR_EL3, Secure Configuration Register (EL3), Page 2648 of AArch64-Reference-Manual.
// ***************************************

#define SCR_RESERVED	    		(3 << 4)
#define SCR_RW				(1 << 10)
#define SCR_NS				(1 << 0)
#define SCR_VALUE	    	    	(SCR_RESERVED | SCR_RW | SCR_NS)

// ***************************************
// SPSR_EL3, Saved Program Status Register (EL3) Page 389 of AArch64-Reference-Manual.
// ***************************************

#define SPSR_MASK_ALL 			(7 << 6)
#define SPSR_EL1h			(5 << 0)
#define SPSR_VALUE			(SPSR_MASK_ALL | SPSR_EL1h)

// ***************************************
// ESR_EL1, Exception Syndrome Register (EL1). Page 2431 of AArch64-Reference-Manual.
// ***************************************

#define ESR_ELx_EC_SHIFT		26
#define ESR_ELx_EC_SVC64		0x15
#define ESR_ELx_EC_DABT_LOW		0x24

#endif
