#include "msock_internal.h"
#include <assert.h>

DLL_LOCAL struct process *process_new(struct domain *domain,
				      msock_callback_t receive_callback,
				      void *receive_data,
				      int procopt)
{
	struct process *process = cache_malloc(&domain->cache_processes,
					       struct process);
	memset(process, 0, sizeof(struct process));

	process->domain = domain;
	unsigned long poff = umap_add(domain->poff_to_process, process);
	if (poff == 0) {
		fatal("Not enough slots for new processes!");
	}
	process->pid = poff_gid_to_pid(poff, domain->gid);
	list_add(&process->in_list,
		 &domain->list_of_processes);
	INIT_QUEUE_HEAD(&process->in_busy_queue);
	INIT_LIST_HEAD(&process->in_hungry_list);
	INIT_QUEUE_ROOT(&process->inbox);
	INIT_QUEUE_ROOT(&process->badmatch);

	process->receive_callback = receive_callback;
	process->receive_data = receive_data;

	if (procopt & PROCOPT_HUNGRY) {
		list_add(&process->in_hungry_list,
			 &domain->list_of_hungry_processes);
	}
	return process;
}


DLL_LOCAL void process_free(struct process *process)
{
	struct domain *domain = process->domain;

	unsigned long poff = pid_to_poff(process->pid);
	umap_del(process->domain->poff_to_process, poff);

	list_del(&process->in_list);
	if (queue_is_enqueued(&process->in_busy_queue)) {
		/* This is slow, but it's still better than not optimizing sends. */
		queue_slow_del(&process->in_busy_queue,
			       &process->domain->queue_of_busy_processes);
	}
	if ( !list_empty(&process->in_hungry_list) ) {
		list_del(&process->in_hungry_list);
	}
	drain_message_queue(domain, &process->inbox);
	drain_message_queue(domain, &process->badmatch);

	process->domain = NULL;
	process->pid = 0;
	process->receive_data = NULL;
	process->receive_callback = NULL;

	cache_free(&domain->cache_processes, struct process, process);
}


DLL_LOCAL void process_run(struct process *process)
{
	_prefetch(process->receive_data);

	while (1) {
		struct queue_head *head = queue_get(&process->inbox);
		if (head == NULL) {
			break;
		}
		struct message *msg = container_of(head, struct message, in_queue);

#ifdef DEBUG_MSG
		printf("recv        <%i:?> --> %s  (type=%s sz=%i)\n",
		       process->domain->gid,
		       pid_tostr(process->pid),
		       msg_type_tostr(msg->msg_type), msg->msg_payload_sz);
#endif

		int recv = process->receive_callback(msg->msg_type,
						     msg->msg_payload,
						     msg->msg_payload_sz,
						     process->receive_data);
		switch (recv) {
		case RECV_OK:
			cache_free(&process->domain->cache_messages,
				   struct message, msg);
			break;
		case RECV_BADMATCH:
			queue_put(&msg->in_queue,
				  &process->badmatch);
			break;
		case RECV_EXIT:
			cache_free(&process->domain->cache_messages,
				   struct message, msg);
			process_free(process);
			return;
		default:
			fatal("wtf?");
		}
	}
}


DLL_PUBLIC void msock_receive(msock_callback_t callback,
			      void *process_data)
{
	struct process *process = get_current_process();
	process->receive_callback = callback;
	process->receive_data = process_data;
	queue_splice_prepend(&process->badmatch,
			     &process->inbox);
}

