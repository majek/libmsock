#ifndef _MSOCK_MPOOL_H
#define _MSOCK_MPOOL_H

#include <string.h>

#define MPOOL_SLOTS 1024
#define MPOOL_ALIGN 8

#define USE_MPOOL

struct mpool_root {
	struct queue_root pool[MPOOL_SLOTS];
};


#define INIT_MPOOL_ROOT(mpool) mpool_root_init(mpool)
DLL_LOCAL void mpool_root_init(struct mpool_root *root);
DLL_LOCAL void mpool_drain(struct mpool_root *root);


#define mpool_malloc(mpool, type)			\
	((type*)mpool_safe_malloc(mpool, sizeof(type)))

#define mpool_free(mpool, type, ptr)				\
	do {							\
		type *a = ptr;					\
		mpool_safe_free(mpool, sizeof(type), a);	\
	} while (0)

static inline int size_to_idx(int size)
{
	return (size+MPOOL_ALIGN-1) / MPOOL_ALIGN;
}

static inline void *mpool_safe_malloc(struct mpool_root *root,
				 int size)
{
#ifdef USE_MPOOL
	int idx = size_to_idx(size);
	if (idx <= ARRAY_SIZE(root->pool)) {
		struct queue_head *head = queue_get(&root->pool[idx]);
		if (head) {
			return head;
		}
		return msock_safe_malloc(size);
	} else {
		return msock_safe_malloc(size);
	}
#else
	return msock_safe_malloc(size);
#endif
}

static inline void mpool_safe_free(struct mpool_root *root,
			      int size,
			      void *ptr)
{
#ifdef USE_MPOOL
	int idx = size_to_idx(size);
	if (idx <= ARRAY_SIZE(root->pool)) {
		struct queue_head *head = (struct queue_head*)ptr;
		queue_put(head,
			  &root->pool[idx]);
	} else {
		msock_safe_free(size, ptr);
	}
#else
	msock_safe_free(size, ptr);
#endif
}


#endif // _MSOCK_MPOOL_H
