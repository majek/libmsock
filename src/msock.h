#ifndef _MSOCK_H
#define _MSOCK_H
#ifdef __cplusplus
extern "C" {
#if 0
} // To make emacs c mode happy. Fuck.
#endif
#endif

#include "config.h"

#include <signal.h>
#include <stdint.h>

typedef void *msock_pid_t;
typedef void *msock_base;

DLL_PUBLIC msock_base msock_base_new(int engines, int max_processes);
DLL_PUBLIC void msock_base_free(msock_base base);


/* Sends a message before entering main loop. */
DLL_PUBLIC void msock_base_send(msock_base base,
				msock_pid_t target,
				int msg_type,
				void* msg_payload, int msg_payload_sz);

/* Sends a message from inside the main event loop. */
DLL_PUBLIC void msock_send(msock_pid_t target,
			   int msg_type,
			   void* msg_payload, int msg_payload_sz);


enum msock_recv {
	RECV_OK = 0xCAFEBABE,
	RECV_BADMATCH,
	RECV_EXIT,
	RECV_OK_YIELD
};

typedef enum msock_recv (*msock_callback_t)(int msg_type,
				void *msg_payload, int msg_payload_sz,
				void *process_data);

/* Starts new 'process'. */
DLL_PUBLIC msock_pid_t msock_base_spawn(msock_base base,
					msock_callback_t callback,
					void *process_data);

DLL_PUBLIC msock_pid_t msock_spawn(msock_callback_t callback,
				   void *process_data);

/* Who am I? */
DLL_PUBLIC msock_pid_t msock_self();

/* Change the callback (receiver) for current 'process'. */
DLL_PUBLIC void msock_receive(msock_callback_t callback,
			      void *process_data);


DLL_PUBLIC void msock_base_loop(msock_base base);
DLL_PUBLIC void msock_base_loopexit();
DLL_PUBLIC void msock_loopexit();



struct msock_msg_fd {
	int fd;
	msock_pid_t victim;
	unsigned long expires;
};

struct msock_msg_signal {
	int signum;
	msock_pid_t victim;
	siginfo_t siginfo;
};

enum msock_msgs {
	MSG_FD_READ,
	MSG_FD_WRITE,
	MSG_FD_TIMEOUTED,
	MSG_FD_REGISTER_READ,
	MSG_FD_REGISTER_WRITE,
	MSG_FD_UNREGISTER,

	MSG_IO_FSYNC,
	MSG_IO_OPEN,
	MSG_IO_PREAD,

	MSG_SIGNAL_REGISTER,
	MSG_SIGNAL_UNREGISTER,
	MSG_SIGNAL,

//	MSG_TIMER_REGISTER,
//	MSG_TIMER_UNREGISTER,

	MSG_EXIT,
	MSG_GC,
	MSG_QUEUE_EMPTY,

	MSG_USER
};


enum enigine_types {
	MSOCK_ENGINE_MASK_SELECT  = 1 << 2,
	MSOCK_ENGINE_MASK_IO      = 1 << 3,
	MSOCK_ENGINE_MASK_SIGNAL  = 1 << 4
};


/* Values can't have top 5 bits set. They are integers only by accident. */
#define PID_BROADCAST ((msock_pid_t)(0)) /* For internal use only. */
#define PID_SELECT    ((msock_pid_t)(1))
//#define PID_TIMER     ((msock_pid_t)(2))
#define PID_IO        ((msock_pid_t)(3))
#define PID_SIGNAL    ((msock_pid_t)(4))
#define MSOCK_MAX_DOMAINS   (5)


/* Engine public interfaces: */
DLL_PUBLIC extern unsigned long msock_now_msecs; /* Monotonic time in ms */

DLL_PUBLIC void msock_send_msg_fd(int msg_type, int fd,
				  unsigned long timeout_msecs);
DLL_PUBLIC void msock_base_send_msg_fd(msock_base base,
				       msock_pid_t victim,
				       int msg_type, int fd,
				       unsigned long timeout_msecs);

DLL_PUBLIC void msock_io_fsync(int fd);
DLL_PUBLIC void msock_io_open(char *pathname, int flags, int mode);
DLL_PUBLIC void msock_io_pread(int fd, char *buf, uint64_t count, uint64_t offset);

DLL_PUBLIC char* msock_pid_tostr(msock_pid_t pid);

DLL_PUBLIC void msock_base_send_msg_signal(msock_base base,
					   msock_pid_t victim,
					   int msg_type,
					   int signum);
DLL_PUBLIC void msock_memory_collect();
DLL_PUBLIC void msock_memory_stats(unsigned long *used_bytes_ptr);


#ifdef __cplusplus
}
#endif
#endif // _MSOCK_H
