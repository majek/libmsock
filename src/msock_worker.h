#ifndef _MSOCK_WORKER_H
#define _MSOCK_WORKER_H

struct worker {
	struct list_head in_list;
	pthread_t thread;
};

DLL_LOCAL void workers_create(struct base *base, int workers_no);
DLL_LOCAL void workers_join(struct base *base);
DLL_LOCAL void worker_loop(struct base *base);

#endif // _MSOCK_WORKER_H
