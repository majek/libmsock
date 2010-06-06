/*
 * Timer wheel algorithm implementation, mostly copied from Linux kernel 2.6.27.
 * That means the license is GPL.
 *
 * The difference between that and the kernel version:
 *  - Simplified.
 *  - Removed multi cpu support, deferrable and other useless stuff.
 *  - The api is more similar to lists.h.
 */
#include "timer.h"

/* Some definitions, orginally located in header. Now there's no point in
 * sharing this stuff as a public api in header. */
#define NEXT_TIMER_MAX_DELTA    ((1UL << 30) - 1)

// from linux/kernel.h
/*
 * Check at compile time that something is of a particular type.
 * Always evaluates to 1 so you may use it easily in comparisons.
 */
#define typecheck(type,x) \
({      type __dummy; \
        typeof(x) __dummy2; \
        (void)(&__dummy == &__dummy2); \
        1; \
})

// from linux/jiffies.h
/*
 *      These inlines deal with timer wrapping correctly. You are
 *      strongly encouraged to use them
 *      1. Because people otherwise forget
 *      2. Because if the timer wrap changes in future you won't have to
 *         alter your driver code.
 *
 * time_after(a,b) returns true if the time a is after time b.
 *
 * Do this with "<0" and ">=0" to only test the sign of the result. A
 * good compiler would generate better code (and a really good compiler
 * wouldn't care). Gcc is currently neither.
 */
#define time_after(a,b)         \
        (typecheck(unsigned long, a) && \
         typecheck(unsigned long, b) && \
         ((long)(b) - (long)(a) < 0))
#define time_before(a,b)        time_after(b,a)

#define time_after_eq(a,b)      \
        (typecheck(unsigned long, a) && \
         typecheck(unsigned long, b) && \
         ((long)(a) - (long)(b) >= 0))
#define time_before_eq(a,b)     time_after_eq(b,a)



TIMER_PUBLIC void init_timer_base(struct timer_base *base,
				  unsigned long long jiffies)
{
	int j;
	for (j = 0; j < TVN_SIZE; j++) {
		INIT_LIST_HEAD(base->tv5.vec + j);
		INIT_LIST_HEAD(base->tv4.vec + j);
		INIT_LIST_HEAD(base->tv3.vec + j);
		INIT_LIST_HEAD(base->tv2.vec + j);
	}
	for (j = 0; j < TVR_SIZE; j++)
		INIT_LIST_HEAD(base->tv1.vec + j);

	base->timer_jiffies = jiffies;
}

TIMER_PUBLIC void timer_add(struct timer_head *timer,
			    unsigned long expires,
			    struct timer_base *base)
{
	timer_mod(timer,
		  expires,
		  base);
}

static void __internal_add_timer(struct timer_base *base,
			  struct timer_head *timer)
{
	unsigned long expires = timer->expires;
	unsigned long idx = expires - base->timer_jiffies;
	struct list_head *vec;

	if (idx < TVR_SIZE) {
		int i = expires & TVR_MASK;
		vec = base->tv1.vec + i;
	} else if (idx < 1 << (TVR_BITS + TVN_BITS)) {
		int i = (expires >> TVR_BITS) & TVN_MASK;
		vec = base->tv2.vec + i;
	} else if (idx < 1 << (TVR_BITS + 2 * TVN_BITS)) {
		int i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
		vec = base->tv3.vec + i;
	} else if (idx < 1 << (TVR_BITS + 3 * TVN_BITS)) {
		int i = (expires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
		vec = base->tv4.vec + i;
	} else if ((signed long) idx < 0) {
		/*
		 * Can happen if you add a timer with expires == jiffies,
		 * or you set a timer to go off in the past
		 */
		vec = base->tv1.vec + (base->timer_jiffies & TVR_MASK);
	} else {
		int i;
		/* If the timeout is larger than 0xffffffff on 64-bit
		 * architectures then we use the maximum timeout:
		 */
		if (idx > 0xffffffffUL) {
			idx = 0xffffffffUL;
			expires = idx + base->timer_jiffies;
		}
		i = (expires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
		vec = base->tv5.vec + i;
	}
	/*
	 * Timers are FIFO:
	 */
	list_add_tail(&timer->entry, vec);
}

TIMER_PUBLIC void timer_mod(struct timer_head *timer,
			    unsigned long expires,
			    struct timer_base *base)
{
	if (timer_pending(timer)) {
		timer_del(timer);
	}

	timer->expires = expires;
	__internal_add_timer(base, timer);
}

static int cascade(struct timer_base *base, struct tvec *tv, int index)
{
	/* cascade all the timers from tv up one level */
	struct timer_head *timer, *tmp;
	struct list_head tv_list;

	list_replace_init(tv->vec + index, &tv_list);

	/*
	 * We are removing _all_ timers from the list, so we
	 * don't have to detach them individually.
	 */
	list_for_each_entry_safe(timer, tmp, &tv_list, entry) {
		__internal_add_timer(base, timer);
	}

	return index;
}


#define INDEX(N) ((base->timer_jiffies >> (TVR_BITS + (N) * TVN_BITS)) & TVN_MASK)

TIMER_PUBLIC int timers_run(struct timer_base *base,
			    unsigned long jiffies)
{
	if (!time_after_eq(jiffies, base->timer_jiffies)) {
		return 0;
	}

	struct timer_head *timer;

	int expired = 0;
	while (time_after_eq(jiffies, base->timer_jiffies)) {
		struct list_head work_list;
		struct list_head *head = &work_list;
		int index = base->timer_jiffies & TVR_MASK;

		/*
		 * Cascade timers:
		 */
		if (!index &&
		    (!cascade(base, &base->tv2, INDEX(0))) &&
		    (!cascade(base, &base->tv3, INDEX(1))) &&
		    !cascade(base, &base->tv4, INDEX(2))) {
			cascade(base, &base->tv5, INDEX(3));
		}
		++base->timer_jiffies;
		list_replace_init(base->tv1.vec + index, &work_list);
		while (!list_empty(head)) {
			timer_cb_t fn;
			struct timer_head *data;

			timer = list_first_entry(head, struct timer_head, entry);
			fn = timer->callback;
			data = timer;
			timer_del(timer);

			fn(data);
			expired++;
		}
	}
	return expired;
}

TIMER_PUBLIC unsigned long timer_next_interrupt(struct timer_base *base)
{
	unsigned long timer_jiffies = base->timer_jiffies;
	unsigned long expires = timer_jiffies + NEXT_TIMER_MAX_DELTA;
	int index, slot, array, found = 0;
	struct timer_head *nte;
	struct tvec *varray[4];

	/* Look for timer events in tv1. */
	index = slot = timer_jiffies & TVR_MASK;
	do {
		list_for_each_entry(nte, base->tv1.vec + slot, entry) {
			found = 1;
			expires = nte->expires;
			/* Look at the cascade bucket(s)? */
			if (!index || slot < index) {
				goto cascade;
			}
			return expires;
		}
		slot = (slot + 1) & TVR_MASK;
	} while (slot != index);

cascade:
	/* Calculate the next cascade event */
	if (index) {
		timer_jiffies += TVR_SIZE - index;
	}
	timer_jiffies >>= TVR_BITS;

	/* Check tv2-tv5. */
	varray[0] = &base->tv2;
	varray[1] = &base->tv3;
	varray[2] = &base->tv4;
	varray[3] = &base->tv5;

	for (array = 0; array < 4; array++) {
		struct tvec *varp = varray[array];

		index = slot = timer_jiffies & TVN_MASK;
		do {
			list_for_each_entry(nte, varp->vec + slot, entry) {
				found = 1;
				if (time_before(nte->expires, expires)) {
					expires = nte->expires;
				}
			}
			/*
			 * Do we still search for the first timer or are
			 * we looking up the cascade buckets ?
			 */
			if (found) {
				/* Look at the cascade bucket(s)? */
				if (!index || slot < index) {
					break;
				}
				return expires;
			}
			slot = (slot + 1) & TVN_MASK;
		} while (slot != index);

		if (index) {
			timer_jiffies += TVN_SIZE - index;
		}
		timer_jiffies >>= TVN_BITS;
	}
	return expires;
}
