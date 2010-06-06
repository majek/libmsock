/* Simple implementation of spin locks. Api based on Linux Kernel. */
#include <pthread.h>
#include <errno.h>

/* How many times to spin, before yielding CPU. */
#define SPINLOCK_ACQUIRE_TRIES 100

/* Valgrind/Helgrind doesn't support spinlocks yet - so we need to emulate
 * them on mutexes. It shouldn't be too hard. */
struct spinlock {
#ifndef HELGRIND
	pthread_spinlock_t l;
#else
	pthread_mutex_t m;
#endif
};

typedef struct spinlock spinlock_t;


#define INIT_SPIN_LOCK(lock) spin_lock_init(lock)

static inline void spin_lock_init(spinlock_t *lock) {
	/* ignore errors, they can't happen in real world */
#ifndef HELGRIND
	pthread_spin_init(&lock->l, PTHREAD_PROCESS_PRIVATE);
#else
	pthread_mutex_init(&lock->m, NULL);
#endif
}

static inline void spin_lock(spinlock_t *lock) {
#ifndef HELGRIND
	int i;
	for (i=0; i < SPINLOCK_ACQUIRE_TRIES; i++) {
		if(EBUSY != pthread_spin_trylock(&lock->l)) {
			return;
		}
	}
	while (1) {
		sched_yield();
		if(EBUSY != pthread_spin_trylock(&lock->l)) {
			return;
		}
	}
#else
	pthread_mutex_lock(&lock->m);
#endif
}

static inline void spin_unlock(spinlock_t *lock) {
#ifndef HELGRIND
	pthread_spin_unlock(&lock->l);
#else
	pthread_mutex_unlock(&lock->m);
#endif
}
