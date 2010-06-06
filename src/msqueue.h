#ifndef MSQUEUE_H
#define MSQUEUE_H
/*
 * Simple, memory-efficient implementation of a lock-free queue.
 *
 * Take a look on the "Simple, Fast and Practical Non-Blocking
 * Concurrent Queue Alorithms" paper by Maged Michael and Michael Scott.
 * It explains where the complexity lays.
 *
 * I tried to implement the algorithm described there. With the simplification
 * which affect the avoidance ABA problem. So we _are_ affected by ABA. Beware.
 *
 * The main reasons that lead us to create this implementation:
 *       - Well tested implementation, that doesn't fight with Helgrind.
 *       - Low memory footprint.
 */

#include <stdlib.h>

#define MSQUEUE_POISON1 0xCAFEBAB5

#ifndef _cas
# define _cas(ptr, oldval, newval) __sync_bool_compare_and_swap(ptr, oldval, newval)
#endif

struct msqueue_head {
	struct msqueue_head *next;
};

struct msqueue_root {
	struct msqueue_head *head;
	struct msqueue_head *tail;

	struct msqueue_head divider;
};

static inline void INIT_MSQUEUE_ROOT(struct msqueue_root *root)
{
	root->divider.next = NULL;
	root->head = &root->divider;
	root->tail = &root->divider;
}

static inline void INIT_MSQUEUE_HEAD(struct msqueue_head *head)
{
	head->next = (void*)MSQUEUE_POISON1;
}

static inline int msqueue_is_enqueued(struct msqueue_head *head)
{
	if (head->next == (void*)MSQUEUE_POISON1) {
		return 1;
	}
	return 0;
}

static inline void msqueue_put(struct msqueue_head *new,
			     struct msqueue_root *root)
{
	if ( !_cas(&new->next, new->next, NULL) ) {
		abort();
	}

	struct msqueue_head *tail;
	struct msqueue_head *next;
	while (1) {
		tail = root->tail;
		next = tail->next;
		if (tail != root->tail) {
			continue;
		}
		if (next == NULL) {
			if (_cas(&tail->next, next, new)) {
				break;
			}
		} else {
			_cas(&root->tail, tail, next);
		}
	}
	_cas(&root->tail, tail, new);
}

static inline struct msqueue_head *msqueue_get(struct msqueue_root *root)
{
	while (1) {
		struct msqueue_head *head = root->head;
		struct msqueue_head *tail = root->tail;
		struct msqueue_head *next = head->next;
		if (head != root->head) {
			continue;
		}
		if (head == tail) {
			if (next == NULL) {
				return NULL;
			}
			_cas(&root->tail, tail, next);
		} else {
			if (_cas(&root->head, head, next)) {
				if (head == &root->divider) {
					msqueue_put(&root->divider, root);
					continue;
				}
				if( !_cas(&head->next, next, (void*)MSQUEUE_POISON1)) {
					abort();
				}
				return head;
			}
		}
	}
}

#endif // MSQUEUE_H
