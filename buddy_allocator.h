#if !defined(__BUDDY_H)
#define __BUDDY_H

#include <stddef.h>

typedef struct {
    size_t len;
    bool   is_free;
} bblock_t;

typedef struct {
    bblock_t *head;
    bblock_t *tail;
    size_t    align;
} buddy_allocator_t;

 /**
 * Create a buddy allocator
 * @param raw_memory Backing memory
 * @param memory_size Backing memory size
 * @return the new buddy allocator
 */
extern buddy_allocator_t * buddy_allocator_create (void *raw_memory , size_t raw_memory_size );

/**
 * Destroy a buddy allocator
 * @param buddy_allocator
 */
extern void buddy_allocator_destroy ( buddy_allocator_t * buddy_allocator );
 /**
 * Allocate memory
 * @param buddy_allocator The buddy allocator
 * @param size Size of memory to allocate
 * @return pointer to the newly allocated memory , or @a NULL if out of memory
 */
extern  void * buddy_allocator_alloc ( buddy_allocator_t * buddy_allocator , size_t size );
 
 /**
 * Deallocates a perviously allocated memory area.
 * If @a ptr is @a NULL , it simply returns
 * @param buddy_allocator The buddy allocator
 * @param ptr The memory area to deallocate
 */
extern void buddy_allocator_free ( buddy_allocator_t * buddy_allocator , void *ptr );
#endif // __BUDDY_H
