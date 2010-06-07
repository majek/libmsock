#ifndef _MSOCK_DOMAIN_H
#define _MSOCK_DOMAIN_H

struct domain {
	spinlock_t lock;
	struct umap_root *poff_to_process;
	struct queue_root queue_of_busy_processes;

	struct mem_cache cache_messages;
	struct mem_cache cache_processes;

	spinlock_t remote_inbox_lock;
	struct queue_root remote_inbox;

	struct queue_root local_inbox;

	struct engine_proto *proto;
	void *ingress_callback_data;

	struct base *base;
	struct list_head in_list;
	struct msqueue_head in_queue;
	int gid;

	struct list_head list_of_processes;
	struct list_head list_of_hungry_processes;

	struct queue_root outbox[MAX_DOMAINS];
};

DLL_LOCAL struct domain *domain_new(struct base *base,
				    struct engine_proto *proto,
				    void *inbound_callback_data,
				    int max_processes);

DLL_LOCAL void domain_free(struct domain *domain);
DLL_LOCAL int domain_run(struct domain *domain);
DLL_LOCAL int count_hungry_domains(struct base *base);

DLL_LOCAL int dispatch_msg_local(struct domain *domain, struct message *msg);


DLL_LOCAL extern __thread struct process *_current_process;

static inline struct process *get_current_process()
{
	if(unlikely(_current_process == NULL)) {
		fatal("Context dependent routine called without a context.");
	}
	return _current_process;
}


#define MAX_MSG_PAYLOAD_SZ (256-8*4)

struct message {
	struct queue_head in_queue;
	msock_pid_t target;
	int msg_type;
	int msg_payload_sz;
	char msg_payload[MAX_MSG_PAYLOAD_SZ];
};

#endif // _MSOCK_DOMAIN_H
