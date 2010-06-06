

#define TW_DEPTH 4
#define TW_VEC_BITS 7
#define TW_VEC_SIZE (1 << (TW_VEC_SIZE))
#define TW_VEC_MASK (TW_VEC_SIZE - 1)

struct timer_root {
	u64 now;
	struct list_head vec[TW_DEPTH][TW_VEC_SIZE];
};

struct timer_head {
	struct list_head in_vec;
	u64 expires;
};

static inline timer_init(struct timer_root *root)
{
	int i, j;
	for (i=0; i < ARRAY_SIZE(root->vec); i++) {
		for (j=0; j < ARRAY_SIZE(root->vec[0]); j++) {
			INIT_LIST_HEAD(&root->vec[i][j])
		}
	}
}

static inline timerwheel_add(struct timer_head *head,
			     u64 timer_delay,
			     struct timer_root *root)
{
	if (expire >= (1 << (TW_DEPTH * TW_VEC_BITS))) {
		fatal("Too big timer");
	}
	head->expire = root->now + td;

	int depth;
	for (depth=TW_DEPTH-1; depth >= 0; depth--) {
		u64 mask = TW_VEC_MASK << (TW_VEC_BITS * depth);
		if ((now & mask) == (td & mask)) {
			continue;
		} else {
			break;
		}
	}
	if (depth == TW_DEPTH-1) {
		fatal("depth too big?\n");
	}

	root->vec[]
}

static inline void timerwheel_del(struct timer_head *head)
{
	list_del(&head->in_vec);
}

typedef (void)(*expiry_fun_t)(struct timer_root *root,
			      struct timer_head *timer);

static inline void timerwheel_tick(struct timer_root *root,
				   expiry_fun_t callback,
				   u64 max_incr)
{
	struct list_head *expired = \
		&root->vec[0][root->now & TW_VEC_MASK];
	struct list_head *head, *safe;
	list_for_each_safe(head, safe, expired) {
		list_del(head);
		struct timer_head *timer = \
			container_of(head, struct timer_head, in_vec);
		callback(root, timer);
	}

	root->now++;
	/* wrapps? */
	if (root->now & TW_VEC_MASK == 0) {
		struct list_head *to_dispatch = \
			&root->vec[1][root->now & (TW_VEC_MASK << (TW_VEC_BITS*1))];
		list_for_each_safe(head, safe, expired) {
			struct timer_head *timer =			\
				container_of(head, struct timer_head, in_vec);
			list_del(&timer->head);
			list_add(&timer->head,
				 &root->vec[0][timer->expires & TW_VEC_MASK]);
		}
	}
}

static inline void timerwheel_trigger(struct timer_root *root,
				      u64 now,
				      expiry_fun_t callback)
{
	while (root->now < now) {
		timerwheel_tick(root, callback, now - root->now);
	}
}
