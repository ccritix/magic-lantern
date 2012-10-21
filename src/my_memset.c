#include "compiler.h"
#include "dryos.h"

void* memset32(void* dest, int val, size_t n)
{
    dest = (void*)((intptr_t)dest & ~3);
    uint32_t* dst = (uint32_t*) dest;
    uint32_t v = val;
    for(size_t i = 0; i < n/4; i++)
        *dst++ = v;
    return (void*)dst;
}

// uses less memory than the one in libc.a
void* memset(void* dest, int val, size_t n)
{
    uint8_t* dst = (int*) dest;
    uint8_t v = val;
    for(size_t i = 0; i < n; i++)
        *dst++ = v;
    return (void*)dst;
}
