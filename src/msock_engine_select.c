#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include "timer.h"
#include "msock_internal.h"

DLL_PUBLIC unsigned long msock_now_msecs;

struct select_data;

struct local_item {
	msock_pid_t victim;
	struct timer_head timer;
	int fd;
	struct select_data *sd;
};

struct select_data {
	fd_set read_fds;
	fd_set write_fds;
	int pipe_read;
	struct local_item items[__FD_SETSIZE];
	struct timer_base tbase;
};

static enum msock_recv process_callback(int msg_type,
					void *msg_payload,
					int msg_payload_sz,
					void *process_data);
static enum msock_recv process_handle_msg_fd(struct select_data *sd,
					     int msg_type,
					     struct msock_msg_fd *msg);


static void timer_callback(struct timer_head *timer);

static void select_constructor(struct base *base,
			       struct engine_proto *proto,
			       int user_max_processes)
{
	int max_open_files = get_max_open_files();

	if (max_open_files > __FD_SETSIZE) {
		fprintf(stderr, "Open files limit (ulimit -n) greater than "
			"__FD_SETSIZE.  If you want to use more than %i file"
			"descriptors, consider recompiling with higher "
			" __FD_SETSIZE.\n", __FD_SETSIZE);
	}

	struct select_data *sd = type_malloc(struct select_data);
	FD_ZERO(&sd->read_fds);
	FD_ZERO(&sd->write_fds);

	int i;
	for (i=0; i < ARRAY_SIZE(sd->items); i++) {
		INIT_TIMER_HEAD(&sd->items[i].timer, timer_callback);
		sd->items[i].fd = i;
		sd->items[i].sd = sd;
	}

	int pipefd[2];
	if (pipe(pipefd) != 0) {
		pfatal("pipe()");
	}
	sd->pipe_read = pipefd[0];
	msock_now_msecs = now_msecs();
	INIT_TIMER_BASE(&sd->tbase, msock_now_msecs);

	struct domain *domain = domain_new(base, proto,
					   (void*)(long)pipefd[1], 1);
	msock_pid_t pid = spawn(domain, process_callback, sd, PROCOPT_HUNGRY);
	msock_register(domain->base, pid, PID_SELECT);
//	msock_register(domain->base, pid, PID_TIMER);

	struct msock_msg_fd msg;
	msg.fd = sd->pipe_read;
	msg.victim = pid;
	msg.expires = 0;
	process_handle_msg_fd(sd, MSG_FD_REGISTER_READ, &msg);
}

static void select_data_free(struct select_data *sd)
{
	close(sd->pipe_read);
	type_free(struct select_data, sd);
}

static void select_destructor(void *ingress_callback_data)
{
	int pipe_write = (long)ingress_callback_data;
	close(pipe_write);
}


static void select_ingress_callback(void *ingress_callback_data) {
	int pipe_write = (long)ingress_callback_data;
	int r = write(pipe_write, "x", 1);
	if (r == -1) {
		perror("write(pipe)");
	}
}

static struct engine_proto engine_select = {
	.name = "select",
	.constructor = select_constructor,
	.destructor = select_destructor,
	.ingress_callback = select_ingress_callback
};

REGISTER_ENGINE(MSOCK_ENGINE_MASK_SELECT, &engine_select);



static void fd_unregister(struct select_data *sd, int fd)
{
	FD_CLR(fd, &sd->read_fds);
	FD_CLR(fd, &sd->write_fds);
	sd->items[fd].victim = NULL;
	timer_del(&sd->items[fd].timer);
}

static void send_msg_helper(msock_pid_t victim, int msg_type, int fd) {
	struct msock_msg_fd msg;
	msg.fd = fd;
	msg.victim = NULL;
	msock_send(victim, msg_type, (void*)&msg, sizeof(msg));
}

static void timer_callback(struct timer_head *timer) {
	struct local_item *item = \
		container_of(timer, struct local_item, timer);
	struct msock_msg_fd msg;
	msg.fd = item->fd;
	msg.victim = NULL;
	msock_send(item->victim, MSG_FD_TIMEOUTED, (void*)&msg, sizeof(msg));
	fd_unregister(item->sd, item->fd);
}

static void process_block(struct select_data *sd)
{
	fd_set read_fds;
	fd_set write_fds;

do_select:
	memcpy(&read_fds, &sd->read_fds, sizeof(read_fds));
	memcpy(&write_fds, &sd->write_fds, sizeof(write_fds));
	errno = 0;

	unsigned long delta_msecs = \
		timer_next_interrupt(&sd->tbase) - msock_now_msecs;
	struct timeval tv = {delta_msecs / 1000,
			     (delta_msecs % 1000) * 1000}; // msecs to usecs

	int r = select(__FD_SETSIZE,
		       &read_fds, &write_fds, NULL,
		       &tv);
	if (r == -1) {
		if (errno == EINTR) {
			goto do_select;
		}
		// TODO: handle EBADF nicely
		pfatal("select()");
	} else if (r == 0) {
		// timeout
	} else { // got events
		int fd;
		int hit=0;
		for (fd=0; fd < __FD_SETSIZE && hit < r; fd++) {
			if (FD_ISSET(fd, &read_fds)) {
				hit++;
				if (fd == sd->pipe_read) {
					char buf[32];
					read(fd, buf, sizeof(buf));
				} else {
					send_msg_helper(sd->items[fd].victim,
							MSG_FD_READ, fd);
					fd_unregister(sd, fd);
				}
			}
			if (FD_ISSET(fd, &write_fds)) {
				send_msg_helper(sd->items[fd].victim,
						MSG_FD_WRITE, fd);
				fd_unregister(sd, fd);
				hit++;
			}
		}
	}

	msock_now_msecs = now_msecs();
	timers_run(&sd->tbase, msock_now_msecs);
}

static enum msock_recv process_handle_msg_fd(struct select_data *sd,
					     int msg_type,
					     struct msock_msg_fd *msg)
{
	struct process *victim = msg->victim;
	int fd = msg->fd;
	unsigned long expires = msg->expires;

	if (fd >= __FD_SETSIZE) {
		fatal("File descriptor %i out of rage %i. "
		      "Recompile with higher __FD_SETSIZE\n",
		      fd, __FD_SETSIZE);
	}

	switch(msg_type) {
	case MSG_FD_REGISTER_READ:
	case MSG_FD_REGISTER_WRITE:
		if (msg_type == MSG_FD_REGISTER_READ) {
			FD_SET(fd, &sd->read_fds);
		} else {	/* _WRITE */
			FD_SET(fd, &sd->write_fds);
		}
		sd->items[fd].victim = victim;
		if (expires) {
			timer_add(&sd->items[fd].timer,
				  expires,
				  &sd->tbase);
		} else {
			timer_del(&sd->items[fd].timer);
		}
		break;

	case MSG_FD_UNREGISTER:
		fd_unregister(sd, fd);
		break;
	default:
		fatal("Broken message %#x", msg_type);
	}
	return RECV_OK;
}

static enum msock_recv process_callback(int msg_type,
					void *msg_payload,
					int msg_payload_sz,
					void *process_data)
{
	struct select_data *sd = (struct select_data*)process_data;

	struct msock_msg_fd *msg = (struct msock_msg_fd *)msg_payload;

	switch (msg_type) {
	case MSG_FD_REGISTER_READ:
	case MSG_FD_REGISTER_WRITE:
	case MSG_FD_UNREGISTER:
		process_handle_msg_fd(sd, msg_type, msg);
		break;

	case MSG_EXIT:
		select_data_free(sd);
		return RECV_EXIT;

	case MSG_QUEUE_EMPTY:
		//safe_printf("block start\n");
		process_block(sd);
		//safe_printf("block done\n");
		return RECV_OK;
	default:
		fatal("Broken message %#x", msg_type);
	}

	return RECV_OK;
}


