#include "msock_internal.h"

#include <string.h>
#include <unistd.h>

#include "io.h"


struct io_data {
	int pipe_read;
};

static enum msock_recv process_callback(int msg_type,
					void *msg_payload,
					int msg_payload_sz,
					void *process_data);

static void io_constructor(struct base *base,
			   struct engine_proto *proto,
			   int user_max_processes)
{
	struct io_data *id = type_malloc(struct io_data);

	int pipefd[2];
	if (pipe(pipefd) != 0) {
		pfatal("pipe()");
	}
	id->pipe_read = pipefd[0];

	struct domain *domain = domain_new(base, proto, (void*)(long)pipefd[1], 1);
	msock_pid_t pid = spawn(domain, process_callback, id, PROCOPT_HUNGRY);
	msock_register(domain->base, pid, PID_IO);
}

static void io_data_free(struct io_data *id)
{
	close(id->pipe_read);
	type_free(struct io_data, id);
}

static void io_destructor(void *ingress_callback_data)
{
	int pipe_write = (long)ingress_callback_data;
	close(pipe_write);
}

static void io_ingress_callback(void *ingress_callback_data) {
	int pipe_write = (long)ingress_callback_data;
	int r = write(pipe_write, "x", 1);
	if (r == -1) {
		perror("write(pipe)");
	}
}

static struct engine_proto engine_io = {
	.name = "io",
	.constructor = io_constructor,
	.destructor = io_destructor,
	.ingress_callback = io_ingress_callback
};

REGISTER_ENGINE(MSOCK_ENGINE_MASK_IO, &engine_io);



struct msock_msg_io {
	msock_pid_t victim;
	// open, pread, fsync
	int fd;

	// pread
	char *buf;
	uint64_t count;
	uint64_t offset;

	// open
	char *pathname;
	int flags;
	int mode;

	int ret;
	int saved_errno;
};

static enum msock_recv process_callback(int msg_type,
					void *msg_payload,
					int msg_payload_sz,
					void *process_data)
{
	struct io_data *id = (struct io_data*)process_data;
	struct msock_msg_io *msg = (struct msock_msg_io *)msg_payload;


	struct msock_msg_io out;
	memcpy(&out, msg, sizeof(out));

	errno = 0;
	switch (msg_type) {
	case MSG_IO_FSYNC:
		out.ret = io_fsync(msg->fd, &out.saved_errno);
		break;
	case MSG_IO_PREAD:
		out.count = io_pread(msg->fd,
				     out.buf, msg->count, msg->offset,
				     &out.saved_errno);
		break;
	case MSG_IO_OPEN:
		out.fd = io_open(msg->pathname, msg->flags, msg->mode,
				 &out.saved_errno);
		break;

	case MSG_EXIT:
		io_data_free(id);
		return RECV_EXIT;

	case MSG_QUEUE_EMPTY: {
		//safe_printf("block start io\n");
		char buf[256];
		read(id->pipe_read, buf, sizeof(buf));
		//safe_printf("block done io\n");
		return RECV_OK;}
	default:
		fatal("?");
	}
	msock_send(msg->victim, msg_type, &out, sizeof(out));
	return RECV_OK;
}


static void msg_io_send_helper(int msg_type, struct msock_msg_io *msg)
{
	msock_send(PID_IO,
		   msg_type,
		   msg, sizeof(struct msock_msg_io));
}

DLL_PUBLIC void msock_io_fsync(int fd)
{
	struct msock_msg_io msg;
	memset(&msg, 0, sizeof(msg));

	msg.victim = msock_self();
	msg.fd = fd;
	msg_io_send_helper(MSG_IO_FSYNC, &msg);
}

DLL_PUBLIC void msock_io_open(char *pathname, int flags, int mode)
{
	struct msock_msg_io msg;
	memset(&msg, 0, sizeof(msg));

	msg.victim = msock_self();
	msg.pathname = pathname;
	msg.flags = flags;
	msg.mode = mode;
	msg_io_send_helper(MSG_IO_OPEN, &msg);
}

DLL_PUBLIC void msock_io_pread(int fd, char *buf, uint64_t count, uint64_t offset)
{
	struct msock_msg_io msg;
	memset(&msg, 0, sizeof(msg));

	msg.victim = msock_self();
	msg.fd = fd;
	msg.buf = buf;
	msg.count = count;
	msg.offset = offset;
	msg_io_send_helper(MSG_IO_PREAD, &msg);
}
