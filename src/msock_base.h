#ifndef _MSOCK_BASE_H
#define _MSOCK_BASE_H

struct domain;

struct base {
	struct msqueue_root queue_of_domains;

	// Locking is done inside mem_zones.
	struct mem_zone zone_messages;
	struct mem_zone zone_processes;

	spinlock_t lock;
	struct domain *gid_to_domain[MAX_DOMAINS];
	struct list_head list_of_domains;
	struct list_head list_of_workers;

	msock_pid_t name_to_pid[MAX_REG_NAMES];
};


DLL_LOCAL msock_pid_t spawn(struct domain *domain,
			    msock_callback_t callback,
			    void *process_data,
			    int procopt);

DLL_LOCAL void send_indirect(struct domain *domain,
			     msock_pid_t target,
			     int msg_type,
			     void* msg_payload, int msg_payload_sz);
DLL_LOCAL int send_flush_outbox(struct domain *domain);
DLL_LOCAL void drain_message_queue(struct domain *domain,
				   struct queue_root *msgbox);


#endif // _MSOCK_BASE_H
