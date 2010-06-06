#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "msock.h"


enum msock_recv callback(int msg_type, void *msg_payload, int msg_payload_sz,
			 void *process_data)
{
	switch(msg_type) {
	case MSG_FD_READ: {
		char buf[256], buf2[256];
		int r = read(0, buf, sizeof(buf));
		if (r > 0) {
			snprintf(buf2, sizeof(buf2), ">%.*s<\n", r, buf);
		} else {
			/* done! */
			msock_loopexit();
		}

		write(1, buf2, strlen(buf2));
		msock_send_msg_fd(MSG_FD_REGISTER_WRITE, 1);
		break;}

	case MSG_FD_WRITE:
		msock_send_msg_fd(MSG_FD_REGISTER_READ, 0);
		break;

	case MSG_EXIT:
		return RECV_EXIT;
	default:
		abort();
	}
	return RECV_OK;
}

int main(int argc, char **argv)
{
	msock_base base = msock_base_new(MSOCK_ENGINE_MASK_SELECT |
					 MSOCK_ENGINE_MASK_IO, 32);

	msock_pid_t pid = msock_base_spawn(base, &callback, NULL);
	msock_base_send_msg_fd(base, pid, MSG_FD_REGISTER_READ, 0);

	msock_base_loop(base);

	msock_base_free(base);
	printf("done!\n");

	return 0;
}
