
struct timer_list {
	struct list_head in_list;
	u64 expires;
	void *user_data;
	void (*user_callback)(void *user_data);
};

struct twheel_base {
	u64 time_msecs;
	struct list_head tv0[256];
	struct list_head tv1[256];
	struct list_head tv2[256];
	struct list_head tv3[256];
};



void twheel_add(struct twheel_base *base,
		struct timer_list *timer,
		u32 timeout_msecs)
{
	
}

void twheel_del(struct timer_list *timer)
{
	list_del(&timer->in_list);
}

u64 twheel_min_timeout(struct twheel_base *base)
{

}

void twheel_run_timers(struct twheel_base *base,
		       u64 now_msecs)
{
	
}
