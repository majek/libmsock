#include "msock_internal.h"


DLL_PUBLIC void msock_register(msock_base ubase,
			       msock_pid_t pid,
			       msock_pid_t name)
{
	struct base *base = ubase;

	unsigned long poff = pid_to_poff(name);
	unsigned int gid = pid_to_gid(name);

	if (gid != 0) {
		fatal("Can only register as a zero domain.");
	}
	if (poff >= MAX_REG_NAMES) {
		fatal("Can't register so big name.");
	}
	if (base->name_to_pid[poff] != 0) {
		fatal("Name already taken.");
	}

	spin_lock(&base->lock);
	base->name_to_pid[poff] = pid;
	spin_unlock(&base->lock);
}
