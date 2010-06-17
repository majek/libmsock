/* Shared API between msock_engine_select and msock_engine_epoll. */

#include "msock_internal.h"

DLL_PUBLIC void msock_send_msg_fd(int msg_type,
				  int fd,
				  unsigned long timeout_msecs)
{
	msock_victim_send_msg_fd(msock_self(),
				 msg_type, fd, timeout_msecs);
}

DLL_PUBLIC void msock_base_send_msg_fd(msock_base base,
				       msock_pid_t victim,
				       int msg_type,
				       int fd,
				       unsigned long timeout_msecs)
{
	struct msock_msg_fd msg;
	msg.fd = fd;
	msg.victim = victim;
	if (timeout_msecs) {
		msg.expires = msock_now_msecs + timeout_msecs;
	} else {
		msg.expires = 0;
	}
	msock_base_send(base,
			PID_SELECT,
			msg_type,
			&msg, sizeof(msg));
}

DLL_PUBLIC void msock_victim_send_msg_fd(msock_pid_t victim,
					 int msg_type, int fd,
					 unsigned long timeout_msecs)
{
	struct msock_msg_fd msg;
	msg.fd = fd;
	msg.victim = victim;
	if (timeout_msecs) {
		msg.expires = msock_now_msecs + timeout_msecs;
	} else {
		msg.expires = 0;
	}
	msock_send(PID_SELECT,
		   msg_type,
		   &msg, sizeof(msg));
}

