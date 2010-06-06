#include "msock_internal.h"


struct engine_proto *engine_prototypes[MAX_DOMAINS];

DLL_LOCAL void engines_start(struct base *base, int engine_mask, int max_processes)
{
	int et;
	for (et=0; et < ARRAY_SIZE(engine_prototypes); et++) {
		unsigned int em = 1 << et;
		if (em & engine_mask) {
			struct engine_proto *proto = engine_prototypes[et];
			if (proto == NULL) {
				fatal("Unsupported engine idx=%i mask=%#x", et, em);
			}

			proto->constructor(base, proto, max_processes);
		}
	}
}

DLL_LOCAL void engines_stop(struct base *base)
{
	struct list_head *head, *safe;
	list_for_each_safe(head, safe, &base->list_of_domains) {
		struct domain *domain = \
			container_of(head, struct domain, in_list);
		domain_free(domain);
	}
}

DLL_PUBLIC void msock_register_engine(int engine_mask, struct engine_proto *ep)
{
	int engine_type;
	for (engine_type=0; engine_type < ARRAY_SIZE(engine_prototypes); engine_type++) {
		unsigned int em = 1 << engine_type;
		if (em & engine_mask) {
			if (engine_type < 0
			    || engine_type >= ARRAY_SIZE(engine_prototypes)
			    || engine_prototypes[engine_type] != NULL) {
				fatal("Engine %i already registered.", engine_type);
			}
			engine_prototypes[engine_type] = ep;
		}
	}
}

