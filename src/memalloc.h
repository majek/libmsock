#ifndef _MEMALLOC
#define _MEMALLOC

#include "config.h"
#include "list.h"
#include "upqueue.h"
#include "spinlock.h"

/* Naming:
 *  - chunk - object/item/buffer */
struct mem_zone {
	spinlock_t lock;
	int chunk_size;
	int aligned_chunk_size;
	int chunks_per_page;
	unsigned int alloc_pages;
	unsigned int freed_pages;

	unsigned int free_chunks;
	struct list_head list_of_pages;
};

struct mem_cache {
	struct mem_zone *zone;
	struct queue_root queue_of_free_chunks;
};


// __cacheline_aligned;

DLL_LOCAL void init_mem_zone(struct mem_zone *zone, int chunk_size);
DLL_LOCAL void init_mem_cache(struct mem_cache *cache, struct mem_zone *zone);
DLL_LOCAL void *mem_alloc(struct mem_cache *cache);
DLL_LOCAL void mem_free(struct mem_cache *cache, void *v_chunk);
DLL_LOCAL void mem_gc(struct mem_cache *cache);

#endif // _MEMALLOC
