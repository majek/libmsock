#include "msock_internal.h"

DLL_LOCAL void mpool_root_init(struct mpool_root *root)
{
	int i;
	for (i=0; i < ARRAY_SIZE(root->pool); i++) {
		INIT_QUEUE_ROOT(&root->pool[i]);
	}
}


DLL_LOCAL void mpool_drain(struct mpool_root *root)
{
	int i;
	for (i=0; i < ARRAY_SIZE(root->pool); i++) {
		while (1) {
			struct queue_head *head =
				queue_get(&root->pool[i]);
			if (head == NULL) {
				break;
			}
			msock_safe_free(i * MPOOL_ALIGN, head);
		}
	}
}
