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

struct mem_chunk {
	struct queue_head in_queue;
};


#define INIT_MEM_ZONE(zone, size)		\
	init_mem_zone(zone, size);

#define INIT_MEM_CACHE(cache, zone)		\
	init_mem_cache(cache, zone);

DLL_LOCAL void init_mem_zone(struct mem_zone *zone, int chunk_size);
DLL_LOCAL void zone_free(struct mem_zone *zone);

DLL_LOCAL void init_mem_cache(struct mem_cache *cache, struct mem_zone *zone);
DLL_LOCAL void cache_drain(struct mem_cache *cache);
DLL_LOCAL unsigned long zone_used_bytes(struct mem_zone *zone);

DLL_LOCAL void _cache_fill(struct mem_cache *cache);


#define cache_malloc(cache, type)		\
	( {((type*)_cache_malloc(cache)); })

#define cache_free(cache, type, ptr)				\
	do {							\
		type *a = ptr;					\
		_cache_free(cache, a);				\
	} while (0)

static inline struct mem_page *_page_from_chunk(struct mem_chunk *chunk)
{
	unsigned long v_page = \
		(unsigned long)chunk & ~((unsigned long)PAGE_SIZE-1);
	return (struct mem_page *)v_page;
}

static inline void *_cache_malloc(struct mem_cache *cache)
{
	struct queue_head *head = queue_get(&cache->queue_of_free_chunks);
	if (unlikely(!head)) {
		_cache_fill(cache);
		head = queue_get(&cache->queue_of_free_chunks);
	}
	struct mem_chunk *chunk = \
		container_of(head, struct mem_chunk, in_queue);

	#ifdef VALGRIND
	void *page = _page_from_chunk(chunk);
	VALGRIND_MEMPOOL_ALLOC(page, chunk, cache->zone->chunk_size);
	#endif
	return chunk;
}

static inline void _cache_free(struct mem_cache *cache, void *v_chunk)
{
	struct mem_chunk *chunk = v_chunk;

	#ifdef VALGRIND
	void *page = _page_from_chunk(chunk);
	VALGRIND_MEMPOOL_FREE(page, chunk);
	VALGRIND_MAKE_MEM_DEFINED(chunk, sizeof(struct mem_chunk));
	#endif

	/* Keep warm memory on top of the stack. */
	queue_put_head(&chunk->in_queue,
		       &cache->queue_of_free_chunks);
}




#endif // _MEMALLOC
