#ifndef _ENTRY_H
#define _ENTRY_H

// visible to asm & C
#define S_STACKFRAME      272
// size of all saved registers. ok to be larger than actually used. 
// quest: "two preemptive printers"
#define S_FRAME_SIZE	  0 /* TODO: replace this */

// corresponding to index in the error messages cf irq.c entry_error_messages
#define SYNC_INVALID_EL1t		    0 
#define IRQ_INVALID_EL1t		    1 
#define FIQ_INVALID_EL1t		    2 
#define ERROR_INVALID_EL1t		    3 

#define SYNC_INVALID_EL1h		    4 
#define IRQ_INVALID_EL1h		    5 
#define FIQ_INVALID_EL1h		    6 
#define ERROR_INVALID_EL1h		    7 

#define SYNC_INVALID_EL0_64	   	    8 
#define IRQ_INVALID_EL0_64	    	9 
#define FIQ_INVALID_EL0_64		    10 
#define ERROR_INVALID_EL0_64		11 

#define SYNC_INVALID_EL0_32		    12 
#define IRQ_INVALID_EL0_32		    13 
#define FIQ_INVALID_EL0_32		    14 
#define ERROR_INVALID_EL0_32		15 


#ifndef __ASSEMBLER__
void ret_from_fork(void); // entry.S
#endif
#endif
