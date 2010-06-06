#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define _NANO 1000000000LL

/* Timespec subtraction in nanoseconds */
#define TIMESPEC_NSEC_SUBTRACT(a,b) \
	(((a).tv_sec - (b).tv_sec) * _NANO + (a).tv_nsec - (b).tv_nsec)


#include "msock.h"

#define USR_START MSG_USER+0
#define USR_PING  MSG_USER+1

struct ud {
	msock_pid_t prev;
};

unsigned long long dt0;

enum msock_recv callback(int msg_type, void *msg_payload, int msg_payload_sz,
			 void *process_data)
{
	struct ud *ud= (struct ud*)process_data;

	switch(msg_type) {
	case USR_START:
		//printf("Trigger set\n");
		ud->prev = *((msock_pid_t *)msg_payload);
		break;

 	case USR_PING: {
		long counter = *((long*)msg_payload);
		//printf("%s %li\n", msock_pid_tostr(msock_self()), counter);
		if (counter == 0) {
			//printf("Done!\n");
			msock_loopexit();
			break;
		}
		counter--;
		msock_send(ud->prev, USR_PING, &counter, sizeof(counter));
		break;}

	case MSG_EXIT:
		free(ud);
		return RECV_EXIT;
	default:
		abort();
	}
	return RECV_OK;
}

int main(int argc, char **argv)
{
	int i;
	long msg_total = 10*1000*1000;
	long pid_total = 1*1000;

	msock_base base = msock_base_new(0, pid_total*2);

	struct ud *ud = (struct ud*)calloc(1, sizeof(struct ud));
	msock_pid_t first = msock_base_spawn(base, &callback, ud);
	msock_pid_t prev = first;
	for(i=0; i < pid_total; i++) {
		struct ud *ud = (struct ud*)calloc(1, sizeof(struct ud));
		ud->prev = prev;
		msock_pid_t cur = msock_base_spawn(base, &callback, ud);
		prev = cur;
	}
	msock_base_send(base, first, USR_START, &prev, sizeof(prev));

	msock_base_send(base, first, USR_PING, &msg_total, sizeof(msg_total));


	struct timespec t0, t1;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);
	msock_base_loop(base);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
	long long td = TIMESPEC_NSEC_SUBTRACT(t1, t0);
	printf("%.3fms total, %.3fns per message, %.3fns per process\n",
	       (double)td/1000000,
	       (double)td/msg_total,
	       (double)td/pid_total);

	msock_base_free(base);
	printf("done!\n");

	return 0;
}
