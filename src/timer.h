#ifndef _TIMER_H
#define _TIMER_H
/*
 * Timer wheel algorithm implementation, mostly copied from Linux kernel 2.6.27.
 * That means the license is GPL.
 */

#include "list.h"

#define TIMER_PUBLIC

struct timer_head;

typedef void (*timer_cb_t)(struct timer_head *);

struct timer_head {
	struct list_head entry;
	unsigned long expires;

	timer_cb_t callback;
};

#define TVN_BITS (6)
#define TVR_BITS (8)
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)

struct tvec {
	struct list_head vec[TVN_SIZE];
};

struct tvec_root {
	struct list_head vec[TVR_SIZE];
};

struct timer_base {
	unsigned long timer_jiffies;
	struct tvec_root tv1;
	struct tvec tv2;
	struct tvec tv3;
	struct tvec tv4;
	struct tvec tv5;
};

#define INIT_TIMER_HEAD(_timer, _callback)	\
	init_timer_head(_timer, _callback)

#define INIT_TIMER_BASE(_base, _jiffies)	\
	init_timer_base(_base, _jiffies)

TIMER_PUBLIC void init_timer_base(struct timer_base *base,
				  unsigned long long jiffies);
TIMER_PUBLIC void timer_add(struct timer_head *timer,
			    unsigned long expires,
			    struct timer_base *base);
TIMER_PUBLIC int timers_run(struct timer_base *base,
			    unsigned long jiffies);
TIMER_PUBLIC unsigned long timer_next_interrupt(struct timer_base *base);

TIMER_PUBLIC void timer_mod(struct timer_head *timer,
			    unsigned long expires,
			    struct timer_base *base);

static inline int timer_pending(const struct timer_head *timer)
{
	return !list_empty(&timer->entry);
}

static inline void timer_del(struct timer_head *timer)
{
	if (timer_pending(timer)) {
		list_del_init(&timer->entry);
	}
}

static inline void init_timer_head(struct timer_head *timer,
				   timer_cb_t callback)
{
	INIT_LIST_HEAD(&timer->entry);
	timer->callback = callback;
}

#endif // _TIMER_H
