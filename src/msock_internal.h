#ifndef _MSOCK_INTERNAL_H
#define _MSOCK_INTERNAL_H

#include "config.h"

#include <pthread.h>

#include "list.h"
#include "spinlock.h"
#include "umap.h"
#include "upqueue.h"
#include "msqueue.h"

#include "msock.h"

#define MAX_DOMAINS (MSOCK_MAX_DOMAINS)
#define MAX_REG_NAMES (32)

struct mpool;
struct base;
struct engine_proto;
struct message;

static inline unsigned long pid_to_poff(msock_pid_t mpid);

#include "msock_utils.h"
#include "msock_mpool.h"

#include "msock_base.h"
#include "msock_domain.h"
#include "msock_engine.h"
#include "msock_engine_user.h"
#include "msock_process.h"
#include "msock_reg.h"
#include "msock_worker.h"


#define GID_BITS 5
#define POFF_BITS ((sizeof(long)*8)-GID_BITS)

static inline unsigned int pid_to_gid(msock_pid_t mpid) {
	unsigned long pid = (unsigned long)mpid;
	return (pid >> POFF_BITS);
}

static inline unsigned long pid_to_poff(msock_pid_t mpid) {
	unsigned long pid = (unsigned long)mpid;
	return (pid << GID_BITS) >> GID_BITS;
}

#define poff_gid_to_pid(poff, gid) \
	(msock_pid_t)( ((unsigned long)(poff)) | \
		       ((unsigned long)(gid) << POFF_BITS) )

enum msock_procopt {
	PROCOPT_HUNGRY = 1 << 1,
};


static inline char *msg_type_tostr(int msg_type) {
	switch (msg_type) {
	case MSG_FD_READ:
		return "MSG_FD_READ";
	case MSG_FD_WRITE:
		return "MSG_FD_WRITE";
	case MSG_FD_REGISTER_READ:
		return "MSG_FD_REGISTER_READ";
	case MSG_FD_REGISTER_WRITE:
		return "MSG_FD_REGISTER_WRITE";
	case MSG_FD_UNREGISTER:
		return "MSG_FD_UNREGISTER";
	case MSG_QUEUE_EMPTY:
		return "MSG_QUEUE_EMPTY";
	case MSG_IO_FSYNC:
		return "MSG_IO_FSYNC";
	case MSG_IO_PREAD:
		return "MSG_IO_PREAD";
	case MSG_IO_OPEN:
		return "MSG_IO_OPEN";
	case MSG_EXIT:
		return "MSG_EXIT";
	default:
		return "MSG_?";
	}
}

static inline char *pid_tostr(msock_pid_t pid) {
	static __thread char buf[32];

	unsigned long poff = pid_to_poff(pid); 
	int gid = pid_to_gid(pid);

	snprintf(buf, sizeof(buf), "<%i:%02lu>", gid, poff);
	return buf;
}

#endif // _MSOCK_INTERNAL_H
