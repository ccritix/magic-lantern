
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* from http://simplestcodings.blogspot.de/2010/08/custom-malloc-function-implementation.html */

typedef struct
{
    int is_available;
    uint32_t size;
} MCB, *MCB_P;


char *mem_start_p;
int max_mem;
int allocated_mem; /* this is the memory in use. */
int mcb_count;

char *heap_end;

enum {NEW_MCB=0,NO_MCB,REUSE_MCB};
enum {FREE,IN_USE};

void malloc_init(void *ptr, uint32_t size)
{
    /* store the ptr and size_in_bytes in global variable */
    max_mem = size;
    mem_start_p = ptr;
    mcb_count = 0;
    allocated_mem = 0;
    heap_end = mem_start_p + size;
    memset(mem_start_p,0x00,max_mem);
}

void *malloc(size_t elem_size)
{
    /* check whether any chunk (allocated before) is free first */
    MCB_P p_mcb;
    int flag = NO_MCB;
    size_t sz;

    p_mcb = (MCB_P)mem_start_p;

    sz = sizeof(MCB);
    
    if(elem_size & 0x0F)
    {
        elem_size += 0x10;
        elem_size &= ~0x0F;
    }

    if( (elem_size + sz) > (max_mem - (allocated_mem + mcb_count * sz ) ) )
    {
        return NULL;
    }
    while( heap_end > ( (char *)p_mcb + elem_size + sz) )
    {
        if ( p_mcb->is_available == 0)
        {
            if( p_mcb->size == 0)
            {
                flag = NEW_MCB;
                break;
            }
            if( p_mcb->size >= (elem_size + sz) )
            {
                flag = REUSE_MCB;
                break;
            }
        }
        p_mcb = (MCB_P) ( (char *)p_mcb + p_mcb->size);
    }

    if( flag != NO_MCB)
    {
        p_mcb->is_available = 1;

        if( flag == NEW_MCB)
        {
            p_mcb->size = elem_size + sizeof(MCB);
        }
        else if( flag == REUSE_MCB)
        {
            elem_size = p_mcb->size - sizeof(MCB);
        }
        mcb_count++;
        allocated_mem += elem_size;
        return ( (char *) p_mcb + sz);
    }

    return NULL;
}

void free(void *p)
{
    /* Mark in MCB that this chunk is free */
    MCB_P ptr = (MCB_P)p;
    ptr--;

    if(ptr->is_available != FREE)
    {
        mcb_count--;
        ptr->is_available = FREE;
        allocated_mem -= (ptr->size - sizeof(MCB));
    }
}
