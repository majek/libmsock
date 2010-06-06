
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "msock_internal.h"
#include "upqueue.h"
#include "list.h"


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

DLL_LOCAL struct umap_root *umap_new(size_t map_sz, ulong max_counter)
{
	struct umap_root *root =  \
		(struct umap_root *)calloc(1, sizeof(struct umap_root));

	root->counter = 1;
	root->max_counter = max_counter;
	root->map_sz = map_sz;
	root->data = (struct umap_data*)calloc(map_sz, sizeof(struct umap_data));
	root->meta = (struct umap_meta*)calloc(map_sz, sizeof(struct umap_meta));
	INIT_QUEUE_ROOT(&root->free_items);

	int i;
	for (i=0; i < map_sz; i++) {
		struct umap_meta *meta = &root->meta[i];
		meta->idx = i;
		INIT_QUEUE_HEAD(&meta->in_queue);
		queue_put(&meta->in_queue, &root->free_items);
	}
	return root;
}

DLL_LOCAL void umap_free(struct umap_root *root)
{
	free(root->data);
	free(root->meta);
	free(root);
}

/* Allocate new number, return 0 if no slots are free. */
DLL_LOCAL ulong umap_add(struct umap_root *root, void *ptr)
{
	struct queue_head *head = queue_get(&root->free_items);
	if (!head) {
		return 0;
	}

	struct umap_meta *meta = container_of(head, struct umap_meta, in_queue);
	struct umap_data *data = &root->data[meta->idx];
count_again:;
	/* (counter + x) mod sz = idx      (assumption: x != 0) */
	int x = (root->map_sz + meta->idx -
		 (root->counter % root->map_sz)) % root->map_sz;
	if (x == 0) {
		x = root->map_sz;
	}
	root->counter += x;
	if (root->counter >= root->max_counter) {
		root->counter = 0;
		goto count_again;
	}

	data->no = root->counter;
	assert(data->no % root->map_sz == meta->idx);
	assert(x > 0);

	data->ptr = ptr;
	return data->no;
}

DLL_LOCAL void umap_del(struct umap_root *root, ulong no)
{
	struct umap_meta *meta = &root->meta[no % root->map_sz];
	struct umap_data *data = &root->data[meta->idx];
	if (data->no != no) {
		// not registered
		return;
	}
	data->no = 0;
	data->ptr = NULL;
	queue_put(&meta->in_queue, &root->free_items);
}

DLL_LOCAL void *umap_get(struct umap_root *root, ulong no)
{
	struct umap_data *data = &root->data[no % root->map_sz];
	_prefetch(data->ptr);
	if (unlikely(data->no != no)) {
		// not registered
		return NULL;
	}
	return data->ptr;
}
