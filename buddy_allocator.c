#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

/*
 * memory layout
-----------------------------------------~ ... -------------------------------..
|t|    ||t|          ||t|                    ||                 ||
|a| 32 ||a|    64    ||a|         128        ||buddy_allocator_t||avail_array[0]
|g|    ||g|          ||g|                    ||                 ||
-----------------------------------------~ ... -------------------------------..

 ___
| 0 |
| 1 |
| 2 |
| 3 |
| 4 |
.   .
.   .
.   .    __________
| m | ->| bblock_t |-+
      <--------------+

  ^
  |
buddy_allocat->avail_array

*/

typedef struct {
    uint8_t avail:1;
    uint8_t order:7;
} tag_t;

typedef struct bblock_s bblock_t;

typedef struct bblock_s {
    tag_t     tag;
    bblock_t *next;
    bblock_t *prev;
} bblock_t;


#define MIN_BSIZE  32 // next power of 2 for log2(sizeof(bblock_t))

typedef struct {
    size_t  raw_memory_size;
    void   *bblock_memory;
    size_t  bblock_memory_size;
    size_t  max_blocks;
    uint8_t min_order;
    uint8_t m; // last order index
    bblock_t *avail_array;
} buddy_allocator_t;


#define dllist_init(l) \
    (l)->prev = l; \
    (l)->next = l

#define dllist_empty(h) \
    (h == (h)->prev)

#define dllist_insert_head(h, x) \
    (x)->next = (h)->next; \
    (x)->next->prev = x; \
    (x)->prev = h; \
    (h)->next = x

#define dllist_remove(x) \
    (x)->next->prev = (x)->prev; \
    (x)->prev->next = (x)->next;

#define dllist_head(h) \
    (h)->next



uint8_t log2_size(size_t size)
{
    int i;
    
    for(i = 0; (size >>= 1); i++)
        ;

    return i;
}

uint8_t get_order(size_t size)
{
    size += sizeof(tag_t); // tag overhead

    if(size < MIN_BSIZE) {
        size = MIN_BSIZE;
    }

    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size |= size >> 32;
    size++;

    return log2_size(size) - log2_size(MIN_BSIZE);
}
/*
 * gets pointed buddy address
 * @param ba  buddy_allocator instance
 * @param order
 * @return pointer pointed buddy
 */
void *buddy_ptr(buddy_allocator_t *ba, void *ptr, uint8_t order)
{
    return ((ptr - ba->bblock_memory) ^ (1UL << order))
            + ba->bblock_memory;
}

/*
 * reserve memory for given order
 * @param buddy_allocator instance
 * @param order
 */
char *reservation(buddy_allocator_t *ba , uint8_t order)
{
    uint8_t  j;
    bblock_t *bp, *bbp, *temp_bp;
    uint64_t  bit_num;
    bool      found = false;

    for(j = order; j <= ba->m;  j++) {
        bp = &ba->avail_array[j];

        if(!dllist_empty(bp)) { // Rl. [Find block.]
            found = true;

            temp_bp = dllist_head(bp);
            dllist_remove(temp_bp); // R2. [Remove from list.]
            bp = temp_bp;

            ((tag_t *)bp)->avail = 0;
            break;
        }
    }

    if(!found) {
        return NULL;
    }

    do {
        // R3. [Split required?]
        if(j ==  order) {
            ((tag_t *)bp)->order = j;

            return (void*)bp + sizeof(tag_t);
        }

        // R4. [Split.]
        j--;

        bbp = buddy_ptr(ba, bp, ba->min_order + j); // P <- L + 2^j,

        ((tag_t *)bbp)->avail = 1;
        ((tag_t *)bbp)->order = j;

        dllist_insert_head(&ba->avail_array[j], bbp);
    } while(j >= 0);

    return NULL;
}


/*
 * return block for given order to buddy system
 * @param buddy_allocator instance
 * @param order
 */
void *liberation(buddy_allocator_t *ba, void *ptr, uint8_t order)
{
    bblock_t *bbp, *bp;

    bp  = (bblock_t *)ptr;
    bbp = buddy_ptr(ba, ptr, ba->min_order + order);

    while(!(ba->m == bbp->tag.order || bbp->tag.avail == 0 ||
            (bbp->tag.avail && bbp->tag.order != order))) {

        dllist_remove(bbp);

        order++;

        if(bbp < bp) {
            bp = bbp;
        }
    }

    bp->tag.avail = 1;

    dllist_insert_head(&ba->avail_array[order], bp);
}


 /**
 * Create a buddy allocator
 * @param raw_memory Backing memory
 * @param memory_size Backing memory size
 * @return the new buddy allocator instance
 */
buddy_allocator_t * buddy_allocator_create (void *raw_memory , size_t raw_memory_size )
{
    size_t size, reserved_size;
    buddy_allocator_t *buddy_allocator;
    uint8_t max_order = log2_size(raw_memory_size);
    uint8_t min_order = log2_size(MIN_BSIZE);
    uint8_t m, i;
    size_t  max_blocks = 1UL << (max_order - min_order);

    reserved_size = sizeof(buddy_allocator_t) +
                (max_order - min_order + 1) * sizeof(bblock_t);

    max_order = log2_size(raw_memory_size - reserved_size); // calculate it again

    m = max_order - min_order;


    buddy_allocator = raw_memory + raw_memory_size - reserved_size;
    buddy_allocator->raw_memory_size = raw_memory_size;

    buddy_allocator->bblock_memory = raw_memory;
    buddy_allocator->bblock_memory_size = raw_memory_size - reserved_size;
    buddy_allocator->max_blocks = max_blocks;

    buddy_allocator->avail_array = raw_memory +
            buddy_allocator->bblock_memory_size + sizeof(buddy_allocator_t);


    buddy_allocator->min_order = min_order;
    /* initialize array of free lists (14) */

    buddy_allocator->m = m;

    for (i = 0; i <= m; i++) {
        dllist_init(&buddy_allocator->avail_array[i]);
    }

    dllist_insert_head(&buddy_allocator->avail_array[m], // (13)
            (bblock_t *)buddy_allocator->bblock_memory);

    ((tag_t *)raw_memory)->avail = 1; // block at m-th order is avail
    ((tag_t *)raw_memory)->order = m;

    printf("buddy_allocator %p\n", buddy_allocator);

    return buddy_allocator;
}

/**
 * Destroy a buddy allocator
 * @param  buddy_allocator instance
 */
void buddy_allocator_destroy ( buddy_allocator_t * buddy_allocator )
{
    if(buddy_allocator == NULL) {
        errno = -1;
        perror("buddy_allocator_destroy NULL");
    }
}
 /**
 * Allocate memory
 * @param buddy_allocator The buddy allocator
 * @param size Size of memory to allocate
 * @return pointer to the newly allocated memory, or NULL if out of memory
 */
void * buddy_allocator_alloc ( buddy_allocator_t * buddy_allocator , size_t size )
{
    size_t   bsize;
    uint8_t  order;
    void    *p;

    if(size == 0) {
        return NULL;
    }

    order = get_order(size);

    p = reservation(buddy_allocator, order);

    return p;
}
 
 /**
 * Deallocates a previously allocated memory area.
 * @param buddy_allocator The buddy allocator
 * @param ptr The memory area to deallocate
 */
void buddy_allocator_free ( buddy_allocator_t * buddy_allocator , void *p )
{
    uint8_t  k;
    bool     avail;
    bblock_t *bbp;

    p -= sizeof(tag_t);

    avail = ((tag_t *)p)->avail;
    k = ((tag_t *)p)->order;

    liberation(buddy_allocator, p, k);
}

     
