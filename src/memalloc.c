/*
 * Specific allocator, tuned for msock. Used for managing memory for static
 * size objects that are often created and destroyed, like messages and
 * processes.
 *
 * Some assumptions:
 *  - In most cases alloc() and free() need to be O(1).
 *  - Should work more or less like slab from kernel
 *    (ie: allocate and free full pages only).
 *  - The allocator must be work well with threads - message allocated by
 *    one thread will be freed by other thread in many cases.
 *  - Data flow is in most cases symmetric, so if a process receives million
 *    of messages, it'll probably send million messages soon.
 *  - Keeping above in mind - there's little sense in freeing memory
 *    (as there is high chance we'll need it again very soon).
 *  - But if user wants us to free memory - that should be possible.
 *  - User can request full-gc. In most cases user should request full
 *    gc every few seconds.
 */

#include <stdlib.h> // posix_memalign

#include "memalloc.h"
#include "msock_utils.h" // pfatal, cacheline_align


struct mem_page {
	struct list_head in_list;
	struct queue_root queue_of_free_chunks;
	int free_chunks;
};

static void page_alloc(struct mem_zone *zone);
static void page_free(struct mem_zone *zone, struct mem_page *page);

DLL_LOCAL void init_mem_zone(struct mem_zone *zone, int chunk_size)
{
	INIT_SPIN_LOCK(&zone->lock);
	zone->chunk_size = chunk_size;
	zone->aligned_chunk_size = cacheline_align(chunk_size);

	int avail_bytes = PAGE_SIZE \
		- cacheline_align(sizeof(struct mem_page));
	zone->chunks_per_page = avail_bytes / zone->aligned_chunk_size;
	zone->alloc_pages = 0;
	zone->freed_pages = 0;
	zone->free_chunks = 0;

	if (chunk_size < sizeof(struct mem_chunk) ||
	    zone->chunks_per_page < 2) {
		abort();
	}

	INIT_LIST_HEAD(&zone->list_of_pages);
}

DLL_LOCAL void zone_free(struct mem_zone *zone)
{
	if (!list_empty(&zone->list_of_pages)) {
		abort();
	}
	if (zone->freed_pages != zone->alloc_pages) {
		abort();
	}
	if (zone->free_chunks != 0) {
		abort();
	}
}


DLL_LOCAL void init_mem_cache(struct mem_cache *cache, struct mem_zone *zone)
{
	cache->zone = zone;
	INIT_QUEUE_ROOT(&cache->queue_of_free_chunks);
}

/* Gives cache at least 'chunks_per_page' free chunks. */
DLL_LOCAL void _cache_fill(struct mem_cache *cache)
{
	struct mem_zone *zone = cache->zone;
	spin_lock(&zone->lock);

	if (zone->free_chunks < zone->chunks_per_page) {
		page_alloc(zone);
	}
	int chunks = 0;
	struct list_head *head, *safe;
	list_for_each_safe(head, safe, &cache->zone->list_of_pages) {
		struct mem_page *page = \
			container_of(head, struct mem_page, in_list);

		chunks += page->free_chunks;
		queue_splice(&page->queue_of_free_chunks,
			     &cache->queue_of_free_chunks);

		list_del(&page->in_list);
		zone->free_chunks -= page->free_chunks;
		page->free_chunks = 0;

		if (chunks >= zone->chunks_per_page) {
			break;
		}
	}

	spin_unlock(&zone->lock);
}


DLL_LOCAL void cache_drain(struct mem_cache *cache)
{
	struct mem_zone *zone = cache->zone;
	spin_lock(&zone->lock);

	while (1) {
		struct queue_head *head = \
			queue_get(&cache->queue_of_free_chunks);
		if (!head) {
			break;
		}
		struct mem_chunk *chunk = \
			container_of(head, struct mem_chunk, in_queue);
		struct mem_page *page = _page_from_chunk(chunk);
		queue_put_head(&chunk->in_queue,
			       &page->queue_of_free_chunks);
		if (page->free_chunks == 0) {
			list_add(&page->in_list,
				 &zone->list_of_pages);
		}
		page->free_chunks++;
		zone->free_chunks++;

		if (page->free_chunks == zone->chunks_per_page) {
			page_free(zone, page);
		}
	}

	spin_unlock(&zone->lock);
}

static void page_alloc(struct mem_zone *zone)
{
	void *ptr;
	int r = posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE);
	if (r != 0) {
		/* TODO: attempt to reclaim some memory before failing? */
		pfatal("posix_memalign(%i, %i)", PAGE_SIZE, PAGE_SIZE);
	}

	#ifdef VALGRIND
	VALGRIND_CREATE_MEMPOOL(ptr, 0, 0);
	VALGRIND_MAKE_MEM_NOACCESS(ptr + sizeof(struct mem_page),
		PAGE_SIZE - sizeof(struct mem_page));
	#endif

	struct mem_page *page = (struct mem_page*)ptr;
	list_add(&page->in_list,
		 &zone->list_of_pages);
	INIT_QUEUE_ROOT(&page->queue_of_free_chunks);
	page->free_chunks = zone->chunks_per_page;
	zone->free_chunks += page->free_chunks;

	char *chunk_ptr = (char*)ptr + cacheline_align(sizeof(struct mem_page));

	int i;
	for (i=0; i < page->free_chunks; i++) {
		struct mem_chunk *chunk = (struct mem_chunk*)chunk_ptr;
		chunk_ptr += zone->aligned_chunk_size;
		#ifdef VALGRIND
		VALGRIND_MAKE_MEM_DEFINED(chunk, sizeof(struct mem_chunk));
		#endif
		queue_put_head(&chunk->in_queue,
			       &page->queue_of_free_chunks);
	}
	zone->alloc_pages++;
}

static void page_free(struct mem_zone *zone, struct mem_page *page)
{
	if (page->free_chunks != zone->chunks_per_page) {
		abort();
	}

	zone->free_chunks -= page->free_chunks;
	list_del(&page->in_list);
	#ifdef VALGRIND
	VALGRIND_DESTROY_MEMPOOL(page);
	VALGRIND_MAKE_MEM_NOACCESS(page, PAGE_SIZE);
	#endif
	free(page);
	zone->freed_pages++;
}

DLL_LOCAL unsigned long zone_used_bytes(struct mem_zone *zone)
{
	unsigned long pages;
	spin_lock(&zone->lock);
	pages = zone->alloc_pages - zone->freed_pages;
	spin_unlock(&zone->lock);
	return pages * PAGE_SIZE;
}
