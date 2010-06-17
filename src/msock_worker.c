#include "msock_internal.h"

static void *worker_entry_point(void *mbase)
{
	struct base *base = (struct base*)mbase;
	worker_loop(base);
	return NULL;
}

DLL_LOCAL void workers_create(struct base *base, int workers_no)
{
	int i;
	for (i=0; i < workers_no; i++) {
		struct worker *worker = type_malloc(struct worker);

		int r = pthread_create(&worker->thread,
				       NULL, /* default attrs */
				       &worker_entry_point,
				       base);
		if (r != 0) {
			perror("pthread_create()");
			type_free(struct worker, worker);
			continue;
		}
		spin_lock(&base->lock);
		list_add(&worker->in_list,
			 &base->list_of_workers);
		spin_unlock(&base->lock);
	}
}

DLL_LOCAL void workers_join(struct base *base)
{
	struct list_head *head, *safe;
	list_for_each_safe(head, safe, &base->list_of_workers) {
		struct worker *worker = \
			container_of(head, struct worker, in_list);
		spin_lock(&base->lock);
		list_del(&worker->in_list);
		spin_unlock(&base->lock);
		int r = pthread_join(worker->thread, NULL);
		if (r != 0) {
			perror("pthread_join()");
		}
		type_free(struct worker, worker);
	}
}

static void worker_domain_run(struct domain *domain)
{
	int egress_msgs = domain_run(domain);

	/* Haven't send anything abroad and has hungry processes. */
	if (!egress_msgs && !list_empty(&domain->list_of_hungry_processes)) {
		struct list_head *head;
		list_for_each(head, &domain->list_of_hungry_processes) {
			struct process *process = \
				container_of(head, struct process, in_hungry_list);
			send_indirect(domain,
				      process->pid,
				      MSG_QUEUE_EMPTY,
				      NULL, 0);
			/* Just a single run. */
			domain_run(domain);
		}
	}
}

DLL_LOCAL void worker_loop(struct base *base)
{
	while (1) {
		struct msqueue_head *head = \
			msqueue_get(&base->queue_of_domains);
		if (unlikely(head == NULL)) {
			break;
		}
		struct domain *domain = \
			container_of(head, struct domain, in_queue);

		spin_lock(&domain->lock);
		worker_domain_run(domain);
		int empty = list_empty(&domain->list_of_processes);
		spin_unlock(&domain->lock);

		if (unlikely(empty)) {
			/* Don't add to msqueue - that means the
			 * domain will never ever be runned again.
			 * But it will be happily freed after loop
			 * exits. */
		} else {
			msqueue_put(&domain->in_queue,
				    &base->queue_of_domains);
		}
	}
}
