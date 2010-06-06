#include <stdio.h>
#include <string.h>
#include <time.h>

#include "timer.h"

static int counter;

struct item {
	struct timer_head timer;

	char *key;
};

void callback(struct timer_head *timer)
{
	struct item *item = \
		container_of(timer, struct item, timer);
	printf("%i %s\n", counter, item->key);
}

int main(int argc, char **argv)
{
	struct timer_base tvec;
	counter = 0;
	INIT_TIMER_BASE(&tvec, 0);

	while (1) {
		int value;
		char *key;
		int r = scanf("%i %as", &value, &key);
		if (r != 2) {
			break;
		}
		struct item *item = malloc(sizeof(struct item));

		INIT_TIMER_HEAD(&item->timer,
				callback);
		item->key = key;

		timer_add(&item->timer,
			  value,
			  &tvec);
	}

	fprintf(stderr, "Start\n");

	while (1) {
		timers_run(&tvec, counter);
		unsigned long t0 = timer_next_interrupt(&tvec);
		unsigned long delay = t0 - counter;
		//fprintf(stderr, "delay: %lu %lu\n", delay, t0);
		if (delay < 0 || delay > (1 << 29)) {
			break;
		}
		counter += delay;
	}

	fprintf(stderr, "Done after %i cycles\n", counter);
	return 0;
}
