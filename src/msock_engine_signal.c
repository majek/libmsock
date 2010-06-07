/* From pthread_sigmask(3):
 *
 * For sigwait to work reliably, the signals being waited for must be blocked
 * in all threads, not only in the calling thread, since otherwise the POSIX
 * semantics for signal delivery do not guarantee that it’s the thread doing
 * the sigwait that will receive the signal. The best way to achieve this
 * is block those signals before any threads are created, and never unblock
 * them in the program other than by calling sigwait.
 *
 * That means that we need to block signals _before_ running threads.
 * As we don't know which signals user would like to use - we're blocking
 * everything by default. With the exception of obvious ones like SIGSEGV.
 *
 * For example by default Ctrl+C will be swallowed.
 */

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>

#include "msock_internal.h"

#define MAX_SIGNALS (SIGUNUSED+1)


struct local_data {
	int pipe_read;
	sigset_t blocked;
	msock_pid_t victims[MAX_SIGNALS];
};

static enum msock_recv process_callback(int msg_type,
					void *msg_payload,
					int msg_payload_sz,
					void *process_data);

static __thread struct local_data *handler_sd;
static void signal_handler(int signum, siginfo_t *si, void *ucontext)
{
	if (handler_sd == NULL) {
		fatal("Signal %i not blocked correctly!", signum);
	}
	printf("got signal %i\n", signum);
	if (handler_sd->victims[signum] ) {
		printf("sending to %s\n", pid_tostr(handler_sd->victims[signum]));
		struct msock_msg_signal msg;

		msg.signum = signum;
		msg.victim = NULL;
		msg.siginfo = *si;
		msock_send(handler_sd->victims[signum],
			   MSG_SIGNAL,
			   (void*)&msg, sizeof(msg));
	} else {
		// Signal is not handled by anyone - just ignore it.
	}
}


static void engine_constructor(struct base *base,
			       struct engine_proto *proto,
			       int user_max_processes)
{
	struct local_data *sd = type_malloc(struct local_data);

	int pipefd[2];
	if (pipe(pipefd) != 0) {
		pfatal("pipe()");
	}
	sd->pipe_read = pipefd[0];

	// assuming we're not in the threaded environment
	sigfillset(&sd->blocked);
	// there's no point in blocking following signals
	sigdelset(&sd->blocked, SIGKILL);
	sigdelset(&sd->blocked, SIGSTOP);
	sigdelset(&sd->blocked, SIGBUS);
	sigdelset(&sd->blocked, SIGFPE);
	sigdelset(&sd->blocked, SIGILL);
	sigdelset(&sd->blocked, SIGSEGV);
	sigdelset(&sd->blocked, SIGABRT);
	sigdelset(&sd->blocked, 64); /* SIGRT32 is used by valgrind. */

	int r = sigprocmask(SIG_BLOCK, &sd->blocked, NULL);
	if (r != 0) {
		fatal("sigprocmask(SIG_BLOCK, *)");
	}

	int i;
	for (i=1; i < MAX_SIGNALS; i++) {
		if (sigismember(&sd->blocked, i)) {
			struct sigaction sa;
			memset(&sa, 0, sizeof(sa));
			sa.sa_sigaction = &signal_handler;
			sa.sa_mask = sd->blocked; // serialize all signals
			sa.sa_flags = SA_SIGINFO;
			sigaction(i, &sa, NULL);
		}
	}

	struct domain *domain = domain_new(base, proto,
					   (void*)(long)pipefd[1], 1);
	msock_pid_t pid = spawn(domain, process_callback, sd, PROCOPT_HUNGRY);
	msock_register(domain->base, pid, PID_SIGNAL);
	return;
}

static void engine_data_free(struct local_data *sd)
{
	int i;
	for (i=1; i < MAX_SIGNALS; i++) {
		if (sigismember(&sd->blocked, i)) {
			struct sigaction sa;
			memset(&sa, 0, sizeof(sa));
			sa.sa_handler = SIG_DFL;
			sigaction(i, &sa, NULL);
		}
	}

	type_free(struct local_data, sd);
}


static void engine_destructor(void *ingress_callback_data)
{
	int pipe_write = (long)ingress_callback_data;
	close(pipe_write);
}

static void engine_ingress_callback(void *ingress_callback_data)
{
	int pipe_write = (long)ingress_callback_data;
	int r = write(pipe_write, "x", 1);
	if (r == -1) {
		perror("write(pipe)");
	}
}

static struct engine_proto engine_user = {
	.name = NULL,
	.constructor = engine_constructor,
	.destructor = engine_destructor,
	.ingress_callback = engine_ingress_callback
};


REGISTER_ENGINE(MSOCK_ENGINE_MASK_SIGNAL, &engine_user);


static void process_block(struct local_data *sd)
{
	sigset_t nothandled;
	sigemptyset(&nothandled);

	fd_set readfds;

	FD_ZERO(&readfds);
	FD_SET(sd->pipe_read, &readfds);
	errno = 0;

	handler_sd = sd;
	int r = pselect(sd->pipe_read+1,
			&readfds, NULL, NULL,
			NULL, &nothandled);
	handler_sd = NULL;

	if (r == -1) {
		if (errno != EINTR) {
			pfatal("pselect()");
		}
		// do break on signal - to flush message buffer
	} else if (r == 0) {
		// timeout
	} else {
		char buf[32];
		read(sd->pipe_read, buf, sizeof(buf));
	}
}

static enum msock_recv process_callback(int msg_type,
					void *msg_payload,
					int msg_payload_sz,
					void *process_data)
{
	struct local_data *sd = (struct local_data *)process_data;
	struct msock_msg_signal *msg = (struct msock_msg_signal *)msg_payload;

	switch (msg_type) {
	case MSG_SIGNAL_REGISTER:
		sd->victims[msg->signum] = msg->victim;
		break;
	case MSG_SIGNAL_UNREGISTER:
		sd->victims[msg->signum] = NULL;
		break;

	case MSG_EXIT:
		engine_data_free(sd);
		return RECV_EXIT;

	case MSG_QUEUE_EMPTY:
		process_block(sd);
		return RECV_OK;
	default:
		fatal("Broken message %#x", msg_type);
	}

	return RECV_OK;
}

DLL_PUBLIC void msock_base_send_msg_signal(msock_base base,
					   msock_pid_t victim,
					   int msg_type,
					   int signum)
{
	struct msock_msg_signal msg;
	msg.signum = signum;
	msg.victim = victim;

	msock_base_send(base,
			PID_SIGNAL,
			msg_type,
			&msg, sizeof(msg));
}
