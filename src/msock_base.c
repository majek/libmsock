#include <string.h>

#include "msock_internal.h"

DLL_PUBLIC msock_base msock_base_new(int engines, int max_processes)
{
	struct base *base = type_malloc(struct base);

	INIT_MSQUEUE_ROOT(&base->queue_of_domains);
	INIT_SPIN_LOCK(&base->lock);
	INIT_MEM_ZONE(&base->zone_messages, sizeof(struct message));
	INIT_MEM_ZONE(&base->zone_processes, sizeof(struct process));

	INIT_LIST_HEAD(&base->list_of_domains);
	INIT_LIST_HEAD(&base->list_of_workers);

	engines_start(base, engines | MSOCK_ENGINE_MASK_USER, max_processes);

	return (msock_base)base;
}


DLL_PUBLIC void msock_base_free(msock_base mbase)
{
	struct base *base = (struct base *)mbase;

	engines_stop(base);

	zone_free(&base->zone_messages);
	zone_free(&base->zone_processes);

	type_free(struct base, base);
}

inline static void _send_indirect(struct domain *domain,
				  msock_pid_t target,
				  int msg_type,
				  void* msg_payload, int msg_payload_sz)
{
	int gid = pid_to_gid(target);
	if (unlikely(gid == 0)) {
		/* Zero gid means name service. */
		target = name_to_pid(domain->base, target);
		if (unlikely(target == 0)) {
			/* Name not registered. How bad is that? */
			fatal("Name not registered.");
		}
		gid = pid_to_gid(target);
	}

	struct message *msg = cache_malloc(&domain->cache_messages, struct message);
	msg->target = target;
	msg->msg_type = msg_type;
	msg->msg_payload_sz = msg_payload_sz;
	if (unlikely(msg_payload_sz > sizeof(msg->msg_payload))) {
		fatal("Can't handle that big payload. "
		      "Requested %i bytes, available %i.",
		      msg_payload_sz, (int)sizeof(msg->msg_payload));
	}
	if (likely(msg_payload_sz)) {
		fast_memcpy(msg->msg_payload, msg_payload, msg_payload_sz);
	}

	/* sending to my domain? - skip outbox */
	if (likely(gid == domain->gid)) {
		/* This option uses per-domain inbox. */
		/* queue_put(&msg->in_queue, */
		/* 	  &domain->local_inbox); */
		/* Alternatively just go straight into process queue. */
		dispatch_msg_local(domain, msg);
	} else {
		queue_put(&msg->in_queue,
			  &domain->outbox[gid]);
	}

#ifdef DEBUG_MSG
	printf("send_%s <%i:?> --> %s  (type=%s sz=%i)\n",
	       gid == domain->gid ? "local " : "abroad",
	       domain->gid, pid_tostr(target),
	       msg_type_tostr(msg_type), msg_payload_sz);
#endif

#ifdef VALGRIND
	/* Helgrind doesn't like sharing memory without locks. */
	VALGRIND_HG_CLEAN_MEMORY(msg, sizeof(struct message));
#endif
}

DLL_PUBLIC void msock_send(msock_pid_t target,
			   int msg_type,
			   void* msg_payload, int msg_payload_sz)
{
	struct domain *domain = get_current_process()->domain;
	_send_indirect(domain, target, msg_type, msg_payload, msg_payload_sz);
}

DLL_PUBLIC void msock_base_send(msock_base ubase,
				msock_pid_t target,
				int msg_type,
				void* msg_payload, int msg_payload_sz)
{
	struct base *base = ubase;
	/* Doesn't really matter which domain is the source/ */
	struct domain *domain = base->gid_to_domain[1];

	_send_indirect(domain, target, msg_type, msg_payload, msg_payload_sz);
	send_flush_outbox(domain);
}

DLL_LOCAL void send_indirect(struct domain *domain,
			     msock_pid_t target,
			     int msg_type,
			     void* msg_payload, int msg_payload_sz)
{
	_send_indirect(domain, target, msg_type, msg_payload, msg_payload_sz);
}

DLL_LOCAL void drain_message_queue(struct domain *domain, struct queue_root *msgbox)
{
	while (1) {
		struct queue_head *head = queue_get(msgbox);
		if (head == NULL) {
			break;
		}
		struct message *msg = container_of(head, struct message, in_queue);
		cache_free(&domain->cache_messages, struct message, msg);
	}
}

DLL_LOCAL int send_flush_outbox(struct domain *domain)
{
	int counter = 0;
	int gid;
	for (gid=0; gid < ARRAY_SIZE(domain->outbox); gid++) {
		struct queue_root *qr = &domain->outbox[gid];
		if (likely(queue_empty(qr))) {
			continue;
		}
		struct domain *victim = domain->base->gid_to_domain[gid];
		if (unlikely(!victim)) {
			drain_message_queue(domain, qr);
			continue;
		}

		spin_lock(&victim->remote_inbox_lock);
		queue_splice(qr,
			     &victim->remote_inbox);
		spin_unlock(&victim->remote_inbox_lock);
		/* Is enqueued? That means it's free to go - no need to wakeup. */
		if (msqueue_is_enqueued(&victim->in_queue)) {
			victim->proto->ingress_callback(victim->ingress_callback_data);
		}

		counter++;
	}
	return counter;
}

DLL_LOCAL msock_pid_t spawn(struct domain *domain,
			    msock_callback_t callback,
			    void *process_data,
			    int procopt)
{
	struct process *process = process_new(domain,
					      callback, process_data,
					      procopt);
	return process->pid;
}

DLL_PUBLIC msock_pid_t msock_base_spawn(msock_base ubase,
					msock_callback_t callback,
					void *process_data)
{
	struct base *base = ubase;
	/* Doesn't really matter which domain is the source/ */
	struct domain *domain = base->gid_to_domain[1];
	return spawn(domain, callback, process_data, 0);
}

DLL_PUBLIC msock_pid_t msock_spawn(msock_callback_t callback,
				   void *process_data)
{
	struct domain *domain = get_current_process()->domain;
	return spawn(domain, callback, process_data, 0);
}

DLL_PUBLIC msock_pid_t msock_self()
{
	return get_current_process()->pid;
}

DLL_PUBLIC void msock_base_loop(msock_base mbase)
{
	struct base *base = (struct base *)mbase;
	int workers_no = max(0, count_hungry_domains(base)-1);

	workers_create(base, workers_no);
	worker_loop(base);
	workers_join(base);
}

static void send_broadcast(struct domain *src_domain,
			   int msg_type,
			   void *msg_payload,
			   int msg_payload_sz)
{
	struct base *base = src_domain->base;

	spin_lock(&base->lock);
	struct list_head *head, *safe;
	list_for_each_safe(head, safe, &base->list_of_domains) {
		struct domain *domain = \
			container_of(head, struct domain, in_list);
		send_indirect(src_domain,
			      poff_gid_to_pid(PID_BROADCAST, domain->gid),
			      msg_type,
			      msg_payload,
			      msg_payload_sz);
	}
	spin_unlock(&base->lock);
}


DLL_PUBLIC void msock_loopexit()
{
	struct domain *domain = get_current_process()->domain;
	send_broadcast(domain, MSG_EXIT, NULL, 0);
}

DLL_PUBLIC char* msock_pid_tostr(msock_pid_t pid)
{
	return pid_tostr(pid);
}

DLL_PUBLIC void msock_memory_collect()
{
	struct domain *domain = get_current_process()->domain;
	send_broadcast(domain, MSG_GC, NULL, 0);
	send_flush_outbox(domain);
}

DLL_PUBLIC void msock_memory_stats(unsigned long *used_bytes_ptr)
{
	struct base *base = get_current_process()->domain->base;

	unsigned long used_bytes =			\
		zone_used_bytes(&base->zone_messages) +	\
		zone_used_bytes(&base->zone_processes);
	if (used_bytes_ptr) {
		*used_bytes_ptr = used_bytes;
	}
}
