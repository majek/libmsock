#ifndef _MSOCK_REG_H
#define _MSOCK_REG_H

static inline msock_pid_t name_to_pid(struct base *base, msock_pid_t name)
{
	unsigned long poff = pid_to_poff(name);
	spin_lock(&base->lock); /* TODO: drop for performance */
	msock_pid_t r = base->name_to_pid[poff];
	spin_unlock(&base->lock);
	return r;
}

DLL_PUBLIC void msock_register(msock_base ubase,
			       msock_pid_t pid,
			       msock_pid_t name);

#endif // _MSOCK_REG_H
