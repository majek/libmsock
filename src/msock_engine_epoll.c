#include <sys/epoll.h>
#include <unistd.h>

#include "timer.h"
#include "msock_internal.h"


struct local_data;

struct local_item {
	struct list_head in_list;
	int fd;
	int new_mask;
	int epoll_mask;
	msock_pid_t victim;
	struct local_data *sd;
	struct timer_head timer;
};

struct local_data {
	int epfd;
	int pipe_read;
	int map_sz;
	struct local_item *map;

	struct list_head changed;
	struct timer_base tbase;
};

static int process_callback(int msg_type,
			    void *msg_payload,
			    int msg_payload_sz,
			    void *process_data);

static void schedule_change(struct local_data *sd,
			    msock_pid_t victim,
			    int fd,
			    int new_mask,
			    unsigned long expires);

static void timer_callback(struct timer_head *timer);

static void epoll_constructor(struct base *base,
			      struct engine_proto *proto,
			      int user_max_processes)
{
	struct local_data *sd = type_malloc(struct local_data);
	sd->map_sz = get_max_open_files();
	sd->map = (struct local_item*) \
		msock_safe_malloc(sizeof(struct local_item) * sd->map_sz);
	int i;
	for (i=0; i < sd->map_sz; i++) {
		struct local_item *li = &sd->map[i];
		li->fd = i;
		INIT_LIST_HEAD(&li->in_list);
		INIT_TIMER_HEAD(&sd->map[i].timer, timer_callback);
		li->sd = sd;
	}

	sd->epfd = epoll_create(128);
	if (sd->epfd == -1) {
		pfatal("epoll_create()");
	}
	INIT_LIST_HEAD(&sd->changed);

	set_msock_now_msecs();

	INIT_TIMER_BASE(&sd->tbase, msock_now_msecs);


	int pipefd[2];
	if (pipe(pipefd) != 0) {
		pfatal("pipe()");
	}
	set_nonblocking(pipefd[1]);
	sd->pipe_read = pipefd[0];

	struct domain *domain = domain_new(base, proto, (void*)(long)pipefd[1], 1);
	msock_pid_t pid = spawn(domain, process_callback, sd, PROCOPT_HUNGRY);
	msock_register(domain->base, pid, PID_SELECT);

	schedule_change(sd, pid, sd->pipe_read, EPOLLIN, 0);
}

static void epoll_data_free(struct local_data *sd)
{
	close(sd->epfd);
	close(sd->pipe_read);
	msock_safe_free(sizeof(struct local_item) * sd->map_sz, sd->map);
	type_free(struct local_data, sd);
}


static void epoll_destructor(void *ingress_callback_data)
{
	int pipe_write = (long)ingress_callback_data;
	close(pipe_write);
}

static void epoll_ingress_callback(void *ingress_callback_data) {
	int pipe_write = (long)ingress_callback_data;
	int r = write(pipe_write, "x", 1);
	if (r == -1) {
		perror("write(pipe)");
	}
}

static struct engine_proto engine_epoll = {
	.name = "epoll",
	.constructor = epoll_constructor,
	.destructor = epoll_destructor,
	.ingress_callback = epoll_ingress_callback
};

REGISTER_ENGINE(MSOCK_ENGINE_MASK_SELECT, &engine_epoll);


static void schedule_change(struct local_data *sd,
			    msock_pid_t victim,
			    int fd,
			    int new_mask,
			    unsigned long expires)
{
	struct local_item *li = &sd->map[fd];
	li->victim = victim;
	li->new_mask = new_mask;
	if (li->new_mask == li->epoll_mask) {
		if (!list_empty(&li->in_list)) {
			list_del_init(&li->in_list);
		}
	} else {
		if (list_empty(&li->in_list)) {
			list_add_tail(&li->in_list,
				      &sd->changed);
		}
	}
	if (expires) {
		timer_add(&li->timer,
			  expires,
			  &sd->tbase);
	} else {
		timer_del(&li->timer);
	}
}

static void timer_callback(struct timer_head *timer) {
	struct local_item *item = \
		container_of(timer, struct local_item, timer);
	struct msock_msg_fd msg;
	msg.fd = item->fd;
	msg.victim = NULL;
	msock_send(item->victim, MSG_FD_TIMEOUTED, (void*)&msg, sizeof(msg));
	schedule_change(item->sd, item->victim, item->fd, 0, 0);
}

static void send_msg_helper(msock_pid_t victim, int msg_type, int fd) {
	struct msock_msg_fd msg;
	msg.fd = fd;
	msg.victim = NULL;
	msock_send(victim, msg_type, (void*)&msg, sizeof(msg));
}

static void process_block(struct local_data *sd)
{
	int r = 0;
	struct epoll_event ev;

	struct list_head *head, *safe;
	list_for_each_safe(head, safe, &sd->changed) {
		ev.events = 0;
		ev.data.u64 = 0;
		struct local_item *li = \
			container_of(head, struct local_item, in_list);
		list_del_init(&li->in_list);
		if (li->new_mask) {
			ev.events = li->new_mask;
			ev.data.fd = li->fd;
			if (!li->epoll_mask) {
				r = epoll_ctl(sd->epfd, EPOLL_CTL_ADD, li->fd, &ev);
			} else {
				r = epoll_ctl(sd->epfd, EPOLL_CTL_MOD, li->fd, &ev);
			}
		} else {
			if (!li->epoll_mask) {
				fatal("wtf?");
			} else {
				r = epoll_ctl(sd->epfd, EPOLL_CTL_DEL, li->fd, &ev);
				if (r == -1 && errno == EBADF) {
					r = 0;
				}
			}
		}
		li->epoll_mask = li->new_mask;
		if (r != 0) {
			pfatal("epoll_ctl");
		}
	}

do_again:;
	unsigned long delta_msecs = \
		timer_next_interrupt(&sd->tbase) - msock_now_msecs;

	errno = 0;
	struct epoll_event events[256];
	r = epoll_wait(sd->epfd, events, ARRAY_SIZE(events), delta_msecs);
	if (r == -1) {
		if (errno == EINTR) {
			goto do_again;
		}
		pfatal("epoll_wait()");
	} else if (r == 0) {
		// timeout
	} else { // got events
		int i;
		for (i=0; i < r; i++) {
			int fd = events[i].data.fd;
			// printf("epoll fd=%i mask=0x%x %s\n", fd, events[i].events, pid_tostr(sd->map[fd].victim));
			if (events[i].events & EPOLLIN) {
				if (fd == sd->pipe_read) {
					char buf[32];
					read(fd, buf, sizeof(buf));
					continue;
				} else {
					send_msg_helper(sd->map[fd].victim,
							MSG_FD_READ, fd);
				}
			} else if (events[i].events & EPOLLOUT) {
				send_msg_helper(sd->map[fd].victim,
						MSG_FD_WRITE, fd);
			} else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
				send_msg_helper(sd->map[fd].victim,
						MSG_FD_CLOSE, fd);
			} else {
				fatal("ftf?");
			}
			schedule_change(sd, NULL, fd, 0, 0);
		}
	}

	set_msock_now_msecs();
	timers_run(&sd->tbase, msock_now_msecs);

}



static int process_callback(int msg_type,
			    void *msg_payload,
			    int msg_payload_sz,
			    void *process_data)
{
	struct local_data *sd = (struct local_data*)process_data;
	struct msock_msg_fd *msg = (struct msock_msg_fd *)msg_payload;

	// safe_printf("msg %i %i\n", msg_type, msg->fd);
	switch (msg_type) {
	case MSG_FD_REGISTER_READ:
		schedule_change(sd, msg->victim, msg->fd, EPOLLIN, msg->expires);
		break;
	case MSG_FD_REGISTER_WRITE:
		schedule_change(sd, msg->victim, msg->fd, EPOLLOUT, msg->expires);
		break;
	case MSG_FD_UNREGISTER:
		schedule_change(sd, msg->victim, msg->fd, 0, 0);
		break;

	case MSG_EXIT:
		epoll_data_free(sd);
		return RECV_EXIT;

	case MSG_QUEUE_EMPTY:
		// safe_printf("block start\n");
		process_block(sd);
		// safe_printf("block done\n");
		return RECV_OK;
	default:
		fatal("Broken message %#x", msg_type);
	}

	return RECV_OK;
}
