#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "msock.h"

#define DELAY 1234

enum msock_recv callback(int msg_type, void *msg_payload, int msg_payload_sz,
			 void *process_data)
{
	unsigned long used_bytes;

	switch(msg_type) {
	case MSG_FD_READ: {
		char buf[256] = {0};
		int r = read(0, buf, sizeof(buf));
		if (r > 0) {
			printf(">%.*s<\n", r-1, buf);
		} else {
			/* done! */
			msock_loopexit();
		}

		msock_io_fsync(0);
		break;}

	case MSG_IO_FSYNC:
		msock_send_msg_fd(MSG_FD_REGISTER_READ, 0, DELAY);
		break;

	case MSG_FD_TIMEOUTED:
		msock_memory_stats(&used_bytes);
		printf("timeout... (%lu bytes memory used)\n", used_bytes);
		msock_memory_collect();
		msock_send_msg_fd(MSG_FD_REGISTER_READ, 0, DELAY);
		break;

	case MSG_EXIT:
		return RECV_EXIT;

	case MSG_SIGNAL:
		printf("Received CTRL+C. Quitting.\n");
		msock_loopexit();
		break;

	default:
		abort();
	}
	return RECV_OK;
}

int main(int argc, char **argv)
{
	msock_base base = msock_base_new(MSOCK_ENGINE_MASK_SELECT |
					 MSOCK_ENGINE_MASK_IO
					 | MSOCK_ENGINE_MASK_SIGNAL
					 , 32);

	msock_pid_t pid = msock_base_spawn(base, &callback, NULL);
	msock_base_send_msg_fd(base, pid, MSG_FD_REGISTER_READ, 0, DELAY);
	msock_base_send_msg_signal(base, pid, MSG_SIGNAL_REGISTER, SIGINT);
	msock_base_send_msg_signal(base, pid, MSG_SIGNAL_REGISTER, SIGUSR1);

	msock_base_loop(base);

	msock_base_free(base);
	printf("done!\n");

	return 0;
}
