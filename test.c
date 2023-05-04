#include "buddy_allocator.h"

#define MAX_ALOCATIONS 200
#define ALLOC_CYCLES 1000

int main(int argc, char **argv)
{
    buddy_allocator_t *buddy_instance;
    void  *buddy_instance_addr;
    size_t buddy_instance_size;

    size_t size, total_size = 0;
    int    i, repeate;
    char *test_array[MAX_ALOCATIONS];
    char *p;
    
    buddy_instance_size = (1L << 20);
    
    if((buddy_instance_addr = mmap(0, buddy_instance_size, PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        perror("filed to mmap");
        exit(EXIT_FAILURE);
    }

    /* tell the kernel that we are going to do random reads */
    if(madvise(buddy_instance_addr, buddy_instance_size, MADV_RANDOM) ) {
        perror("filed to madvise");
        exit(EXIT_FAILURE);
    }

    printf("buddy_allocator_create(%p, %lu)\n",
            buddy_instance_addr, buddy_instance_size);
    
    buddy_instance = buddy_allocator_create(buddy_instance_addr,
            buddy_instance_size);

    if(buddy_instance == NULL) {
        perror("buddy_allocator_create");
        exit(EXIT_FAILURE);
    }

    size = (1L << 18);
    total_size += size;

    printf("sainty check: alloc %ud\n", size);
    
    p = buddy_allocator_alloc(buddy_instance, size);
    if(p == NULL) {
        errno = -1;
        perror("filed to buddy_allocator_alloc\n");
        exit(EXIT_FAILURE);
    }

    memset(p, 'a', size);

    printf("sainty check: free %ud\n", size);
    buddy_allocator_free(buddy_instance, p);


    printf("loop alloc/free\n");
    for(repeate = 0; repeate < ALLOC_CYCLES; repeate++) {
        for(i = 0; i < MAX_ALOCATIONS; i++) {
            size = 16 * (i + 1);
            printf("try to allocate: %lu bytes\n", size);
            p = buddy_allocator_alloc(buddy_instance, size);

            if(p == NULL) {
                errno = -1;
                perror("filed to buddy_allocator_alloc\n");
                exit(EXIT_FAILURE);
            }

            memset(p, 'a', size);

            printf("pointer to allocated: %p\n", p);

            test_array[i] = p;
            total_size += size;

            printf("total allocated: %lu\n", total_size);
        }

        for(i = 0; i < MAX_ALOCATIONS; i++) {
            p = test_array[i];
            printf("pointer to be freed: %p\n", p);
            buddy_allocator_free(buddy_instance, p);
        }
    }
    
    buddy_allocator_destroy(buddy_instance);

    if(munmap(buddy_instance_addr, buddy_instance_size) < 0) {
        perror("filed to munmap");
    }
    
    exit(EXIT_SUCCESS);
}   
