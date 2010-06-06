#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "msock.h"


enum msock_recv callback(int msg_type, void *msg_payload, int msg_payload_sz,
			 void *process_data)
{
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

		msock_send_msg_fd(MSG_FD_REGISTER_READ, 0, 12345);
		break;}

	case MSG_FD_TIMEOUTED:
		printf("timeout...\n");
		msock_send_msg_fd(MSG_FD_REGISTER_READ, 0, 12345);
		break;

	case MSG_EXIT:
		return RECV_EXIT;
	default:
		printf("msg_type 0x%#x\n", msg_type);
		abort();
	}
	return RECV_OK;
}

int main(int argc, char **argv)
{
	msock_base base = msock_base_new(MSOCK_ENGINE_MASK_SELECT, 32);

	msock_pid_t pid = msock_base_spawn(base, &callback, NULL);
	msock_base_send_msg_fd(base, pid, MSG_FD_REGISTER_READ, 0, 0);

	msock_base_loop(base);

	msock_base_free(base);

	return 0;
}
