// Mutual exclusion lock.
// derived from xv6

#ifndef SPINLOCK_H
#define SPINLOCK_H
struct spinlock {
  unsigned int locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock. 
};


#endif