#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "msock.h"

int callback(int msg_type, void *msg_payload, int msg_payload_sz,
	     void *process_data)
{
	unsigned long used_bytes;

	switch(msg_type) {
	case MSG_EXIT:
		return RECV_EXIT;

	case MSG_SIGNAL: {
		struct msock_msg_signal *sig = (struct msock_msg_signal *)msg_payload;
		msock_memory_stats(&used_bytes);
		printf("Received %i (%lu bytes memory used)\n",
		       sig->signum, used_bytes);
		if(sig->signum == 2) {
			printf("Quitting\n");
			msock_loopexit();
		} else {
			msock_send_msg_signal(MSG_SIGNAL_REGISTER, sig->signum);
		}
		break; }

	default:
		abort();
	}
	return RECV_OK;
}

int main(int argc, char **argv)
{
	msock_base base = msock_base_new(MSOCK_ENGINE_MASK_SIGNAL, 32);

	msock_pid_t pid = msock_base_spawn(base, &callback, NULL);
	msock_base_send_msg_signal(base, pid, MSG_SIGNAL_REGISTER, SIGINT);
	msock_base_send_msg_signal(base, pid, MSG_SIGNAL_REGISTER, SIGUSR1);
	msock_base_send_msg_signal(base, pid, MSG_SIGNAL_REGISTER, SIGHUP);

	msock_base_loop(base);

	msock_base_free(base);
	printf("done!\n");

	return 0;
}
