#ifndef _UMAP_H
#define _UMAP_H

#include "config.h"
#include "upqueue.h"
#include "list.h"

typedef unsigned long ulong;

struct umap_data {
	ulong no;		/* currently allocated number */
	void *ptr;		/* user pointer */
};

struct umap_meta {
	ulong idx;		/* position in table */
	struct queue_head in_queue;
};

struct umap_root {
	ulong counter;		/* max allocated number+1 */
	ulong max_counter;

	struct queue_root free_items;
	size_t map_sz;
	struct umap_data *data;
	struct umap_meta *meta;
};


DLL_LOCAL struct umap_root *umap_new(size_t map_sz, ulong max_counter);
DLL_LOCAL void umap_free(struct umap_root *root);
DLL_LOCAL ulong umap_add(struct umap_root *root, void *ptr);
DLL_LOCAL void umap_del(struct umap_root *root, ulong no);


static inline void *umap_get(struct umap_root *root, ulong no)
{
	struct umap_data *data = &root->data[no % root->map_sz];
	_prefetch(data->ptr);
	if (unlikely(data->no != no)) {
		// not registered
		return NULL;
	}
	return data->ptr;
}

#endif // _UMAP_H
