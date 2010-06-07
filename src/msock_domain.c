#include <string.h>

#include "msock_internal.h"

DLL_LOCAL __thread struct process *_current_process;


static int find_free_gid(struct base *base)
{
	int gid;
	/* gid 0 is reserved for naming/broadcast */
	for (gid=1; gid < ARRAY_SIZE(base->gid_to_domain); gid++) {
		if (base->gid_to_domain[gid] == NULL) {
			return gid;
		}
	}
	fatal("Too many domains.");
	return 0;
}

DLL_LOCAL struct domain *domain_new(struct base *base,
				    struct engine_proto *proto,
				    void *inbound_callback_data,
				    int max_processes)
{
	struct domain *domain = type_malloc(struct domain);

	INIT_SPIN_LOCK(&domain->lock);
	INIT_SPIN_LOCK(&domain->remote_inbox_lock);
	INIT_QUEUE_ROOT(&domain->remote_inbox);

	INIT_QUEUE_ROOT(&domain->local_inbox);

	domain->base = base;
	list_add_tail(&domain->in_list,
		      &base->list_of_domains);
	INIT_MSQUEUE_HEAD(&domain->in_queue);
	domain->gid = find_free_gid(base);
	msqueue_put(&domain->in_queue,
		    &base->queue_of_domains);
	base->gid_to_domain[domain->gid] = domain;

	domain->poff_to_process = umap_new(max_processes,
					   (1L<<(sizeof(off_t)*8-5)) -1);

	INIT_LIST_HEAD(&domain->list_of_processes);
	INIT_LIST_HEAD(&domain->list_of_hungry_processes);
	INIT_QUEUE_ROOT(&domain->queue_of_busy_processes);

	INIT_MEM_CACHE(&domain->cache_messages, &base->zone_messages);
	INIT_MEM_CACHE(&domain->cache_processes, &base->zone_processes);

	int i;
	for (i=0; i < ARRAY_SIZE(domain->outbox); i++) {
		INIT_QUEUE_ROOT(&domain->outbox[i]);
	}

	domain->proto = proto;
	domain->ingress_callback_data = inbound_callback_data;

	return domain;
}

DLL_LOCAL void domain_free(struct domain *domain)
{
	domain->proto->destructor(domain->ingress_callback_data);
	domain->ingress_callback_data = NULL;

	drain_message_queue(domain, &domain->remote_inbox);
	drain_message_queue(domain, &domain->local_inbox);

	domain->base->gid_to_domain[domain->gid] = NULL;
	list_del(&domain->in_list);

	umap_free(domain->poff_to_process);

	cache_drain(&domain->cache_messages);
	cache_drain(&domain->cache_processes);

	type_free(struct domain, domain);
}

static inline void dispatch_msg_single(struct process *process, struct message *msg)
{
	int was_empty = queue_put(&msg->in_queue,
				  &process->inbox);
	if (was_empty) {
		queue_put(&process->in_busy_queue,
			  &process->domain->queue_of_busy_processes);
	}
}

static struct message *message_clone(struct domain *domain, struct message *org)
{
	struct message *dst = cache_malloc(&domain->cache_messages, struct message);
	fast_memcpy(dst, org, sizeof(struct message) -
		    MAX_MSG_PAYLOAD_SZ + org->msg_payload_sz);
	return dst;
}

static void dispatch_msg_broadcast(struct domain *domain, struct message *msg)
{
	struct list_head *head;
	list_for_each(head, &domain->list_of_processes) {
		struct process *process = \
			container_of(head, struct process, in_list);
		struct message *lmsg = message_clone(domain, msg);
		dispatch_msg_single(process, lmsg);
	}
	cache_free(&domain->cache_messages, struct message, msg);
}

static void dispatch_special(struct domain *domain, struct message *msg)
{
	if (msg->msg_type == MSG_GC) {
		cache_free(&domain->cache_messages, struct message, msg);

		cache_drain(&domain->cache_messages);
		cache_drain(&domain->cache_processes);
	} else {
		abort();
	}
}

DLL_LOCAL int dispatch_msg_local(struct domain *domain, struct message *msg)
{
	int counter = 0;
	unsigned long poff = pid_to_poff(msg->target);
	if (likely(poff != 0)) {
		struct process *process = (struct process*)	\
			umap_get(domain->poff_to_process, poff);

		if (likely(process != NULL)) {
			counter ++;
			dispatch_msg_single(process, msg);
		} else { // process == NULL
			cache_free(&domain->cache_messages, struct message, msg);
		}
	} else { // poff == 0,  aka broadcast
		counter ++;
		if (msg->msg_type == MSG_GC) {
			/* special messages - to domain, not to processes */
			dispatch_special(domain, msg);
		} else {
			/* normal broadcast - copy over to everybody */
			dispatch_msg_broadcast(domain, msg);
		}
	}
	return counter;
}

static int dispatch_local_inbox(struct domain *domain)
{
	int counter = 0;
	while (1) {
		struct queue_head *head = queue_get(&domain->local_inbox);
		if (head == NULL) {
			break;
		}
		struct message *msg = \
			container_of(head, struct message, in_queue);
		counter += dispatch_msg_local(domain, msg);
	}
	return counter;
}

static void process_queue_run(struct domain *domain)
{
	while (1) {
		struct queue_head *head = \
			queue_get(&domain->queue_of_busy_processes);
		if (head == NULL) {
			break;
		}
		struct process *process = \
			container_of(head, struct process, in_busy_queue);

		/* Process can get deleted. */
		_current_process = process;
		process_run( process );
		_current_process = NULL;
	}
}

DLL_LOCAL int domain_run(struct domain *domain)
{
	spin_lock(&domain->remote_inbox_lock);
	queue_splice(&domain->remote_inbox,
		     &domain->local_inbox);
	spin_unlock(&domain->remote_inbox_lock);

	dispatch_local_inbox(domain);
	while (1) {
		process_queue_run(domain);

		int delivered = dispatch_local_inbox(domain);
		if (delivered == 0) {
			break;
		}
	}

	return send_flush_outbox(domain);
}

DLL_LOCAL int count_hungry_domains(struct base *base)
{
	int i=0;

	struct list_head *head;
	list_for_each(head, &base->list_of_domains) {
		struct domain *domain = \
			container_of(head, struct domain, in_list);
		if (!list_empty(&domain->list_of_hungry_processes)){
			i++;
		}
	}
	return i;
}
