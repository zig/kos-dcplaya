/* KallistiOS ##version##

   arch/dreamcast/include/spinlock.h
   (c)2001 Dan Potter
   
   $Id: spinlock.h,v 1.6 2002/11/13 02:05:15 bardtx Exp $
*/

#ifndef __ARCH_SPINLOCK_H
#define __ARCH_SPINLOCK_H

/* Defines processor specific spinlocks */

#include <sys/cdefs.h>
__BEGIN_DECLS

/* DC implementation uses threads most of the time */
#include <kos/thread.h>

/* Spinlock data type */
typedef volatile int spinlock_t;

/* Value initializer */
#define SPINLOCK_INITIALIZER 0

/* Initialize a spinlock */
#define spinlock_init(A) *(A) = SPINLOCK_INITIALIZER

/* Note here that even if threads aren't enabled, we'll still set the
   lock so that it can be used for anti-IRQ protection (e.g., malloc) */

/* VP : added that */
/* Try to lock, set B to 1 if success */
#define spinlock_trylock(A, B) do { \
	spinlock_t * __lock = A; \
	int __gotlock = 0; \
		asm volatile ("tas.b @%1\n\t" \
			      "movt %0\n\t" \
				: "=r" (__gotlock) : "r" (__lock) : "t", "memory"); \
		if (!__gotlock) \
			B=0; \
		else B=1; \
} while (0)

/* Spin on a lock */
#define spinlock_lock(A) do { \
	spinlock_t * __lock = A; \
	int __gotlock = 0; \
	while(1) { \
		asm volatile ("tas.b @%1\n\t" \
			      "movt %0\n\t" \
				: "=r" (__gotlock) : "r" (__lock) : "t", "memory"); \
		if (!__gotlock) \
			thd_pass(); \
		else break; \
	} \
} while (0)

/* Free a lock */
#define spinlock_unlock(A) do { \
		*(A) = 0; \
	} while (0)

__END_DECLS

/* Determine if a lock is locked */
#define spinlock_is_locked(A) ( *(A) != 0 )

#endif	/* __ARCH_SPINLOCK_H */

