#include <stdio.h>

#include "msock.h"


enum msock_recv callback(int msg_type, void *msg_payload, int msg_payload_sz,
			 void *process_data)
{
	printf("Msg type %#x\n", msg_type);
	msock_loopexit(NULL);
	return RECV_EXIT;
}

int main()
{
	msock_base base = msock_base_new(MSOCK_ENGINE_MASK_SELECT, 32);

	msock_pid_t pid = msock_base_spawn(base, &callback, NULL);
	msock_base_send(base, pid, 0x333, NULL, 0);

	msock_base_loop(base);

	msock_base_free(base);

	return 0;
}
