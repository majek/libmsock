#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "msock.h"
#include "network.h"

inline int min(int a, int b) {
	if (a <= b) {
		return a;
	}
	return b;
}

struct client_data {
	int fd;
	int buf_sz;
	int buf_len;
	int buf_pos;
	char buf[1024*1024];
};

struct server_data {
	int fd;
};

int client_close(struct client_data *ud)
{
	printf("fd=%i pid=#%s closed\n", ud->fd, msock_pid_tostr(msock_self()));
	close(ud->fd);
	free(ud);
	return RECV_EXIT;
}

int client_read_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		   void *process_data);
int client_write(struct client_data *ud, int block);
int client_write_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		    void *process_data);


int client_read(void *process_data)
{
	struct client_data *ud = (struct client_data*)process_data;
	msock_send_msg_fd(MSG_FD_REGISTER_READ, ud->fd, 0);
	return msock_receive(&client_read_cb, ud);
}

int client_read_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		   void *process_data)
{
	struct client_data *ud = (struct client_data*)process_data;
	int r;

	switch(msg_type) {
	case MSG_FD_READ:
		r = read(ud->fd, ud->buf + ud->buf_len, min(64*1024, ud->buf_sz - ud->buf_len));
		if (r > 0) {
			ud->buf_len += r;
			if (ud->buf[ud->buf_len-1] == '\n' ||
			    ud->buf_len == ud->buf_sz) {
				return client_write(ud, 0);
			} else {
				break;
			}
		}
		// fallthrough
	case MSG_FD_CLOSE:
		return client_close(ud);
	default:
		abort();
	}
	return client_read(ud);
}

int client_write(struct client_data *ud, int block)
{
	if (block) {
		msock_send_msg_fd(MSG_FD_REGISTER_WRITE, ud->fd, 0);
		return msock_receive(&client_write_cb, ud);
	} else {
		return client_write_cb(MSG_FD_WRITE, NULL, 0, ud);
	}
}

int client_write_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		    void *process_data)
{
	struct client_data *ud = (struct client_data*)process_data;
	int r;

	switch(msg_type) {
	case MSG_FD_WRITE:
		r = write(ud->fd, ud->buf+ud->buf_pos, min(64*1024, ud->buf_len-ud->buf_pos));
		if (r >= 0) {
			ud->buf_pos += r;
			if (ud->buf_pos >= ud->buf_len) {
				ud->buf_pos = 0;
				ud->buf_len = 0;
				return client_read(ud);
			} else {
				break;
			}
		}
		// fallthrough
	case MSG_FD_CLOSE:
		return client_close(ud);
	default:
		abort();
	}
	return client_write(ud, 1);
}

int server_read_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		   void *process_data);

int server_read(void *process_data)
{
	struct server_data *sd = (struct server_data*)process_data;
	msock_send_msg_fd(MSG_FD_REGISTER_READ, sd->fd, 0);
	return msock_receive(&server_read_cb, process_data);
}

int server_read_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		void *process_data)
{
	struct server_data *sd = (struct server_data*)process_data;
	switch(msg_type) {
	case MSG_EXIT:
		close(sd->fd);
		free(sd);
		return RECV_EXIT;

	case MSG_FD_READ: {
		struct client_data *ud = \
			(struct client_data*)malloc(sizeof(struct client_data));
		ud->fd = net_accept(sd->fd, NULL, NULL);
		ud->buf_len = 0;
		ud->buf_pos = 0;
		ud->buf_sz = sizeof(ud->buf);

		msock_pid_t pid = msock_spawn2(&client_read, ud);
		printf("fd=%i pid=#%s new\n", ud->fd, msock_pid_tostr(pid));
		break;
	};
	default:
		abort();
	}
	return server_read(sd);
}

int handle_quit_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		   void *process_data);

int handle_quit(void *process_data)
{
	msock_send_msg_signal(MSG_SIGNAL_REGISTER, SIGINT);
	msock_send_msg_signal(MSG_SIGNAL_REGISTER, SIGALRM);
	return msock_receive(&handle_quit_cb, process_data);
}

int handle_quit_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		   void *process_data)
{
	switch(msg_type) {
	case MSG_EXIT:
		return RECV_EXIT;

	case MSG_SIGNAL:
		printf("Received Ctrl+C. Quitting.\n");
		msock_loopexit();
		return RECV_OK;

	default:
		abort();
	}
}


int handle_info_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		   void *process_data);

int handle_info(void *process_data)
{
	msock_send_msg_signal(MSG_SIGNAL_REGISTER, SIGUSR1);
	msock_send_msg_signal(MSG_SIGNAL_REGISTER, SIGUSR2);
	msock_send_msg_signal(MSG_SIGNAL_REGISTER, SIGHUP);
	return msock_receive(&handle_info_cb, process_data);
}

int handle_info_cb(int msg_type, void *msg_payload, int msg_payload_sz,
		   void *process_data)
{
	struct msock_msg_signal *msg = \
		(struct msock_msg_signal *)msg_payload;
	unsigned long used_bytes;

	switch(msg_type) {
	case MSG_EXIT:
		return RECV_EXIT;

	case MSG_SIGNAL:
		msock_memory_stats(&used_bytes);
		printf(" [i] Memory: %lu bytes used.\n", used_bytes);
		msock_memory_collect();

		msock_send_msg_signal(MSG_SIGNAL_REGISTER, msg->signum);
		return RECV_OK;

	default:
		abort();
	}
}


int main() {
	msock_base base = msock_base_new(MSOCK_ENGINE_MASK_SELECT
					 | MSOCK_ENGINE_MASK_SIGNAL,
					 0);

	struct server_data *sd = \
		(struct server_data *)malloc(sizeof(struct server_data));

	sd->fd = net_bind("0.0.0.0", 1234);
	if (sd->fd < 0) {
		printf("can't bind\n");
		return -1;
	}
	msock_base_spawn2(base, &server_read, sd);
	msock_base_spawn2(base, &handle_quit, NULL);
	msock_base_spawn2(base, &handle_info, NULL);

	msock_base_loop(base);

	msock_base_free(base);
	printf("done!\n");

	return 0;
}
