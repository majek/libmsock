#ifndef _MSOCK_PROCESS_H
#define _MSOCK_PROCESS_H

struct process {
	struct msqueue_root qqq;

	struct domain *domain;
	msock_pid_t pid;
	struct list_head in_list;
	struct list_head in_hungry_list;
	struct queue_head in_busy_queue;

	struct queue_root inbox;
	struct queue_root badmatch;

	msock_callback_t receive_callback;
	void *receive_data;
};

DLL_LOCAL struct process *process_new(struct domain *domain,
				      msock_callback_t receive_callback,
				      void *receive_data,
				      int procopt);
DLL_LOCAL void process_free(struct process *process);
DLL_LOCAL void process_run(struct process *process);



#endif // _MSOCK_PROCESS_H
