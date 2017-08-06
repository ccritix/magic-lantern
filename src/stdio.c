/** \file
 * Re-implementation of <stdio.h> functions that we don't have in the
 * Canon firmware.
 *
 * These are decidedly non-optimal.
 *
 * Portions copied from uClibc 0.9.30 under the GPL.
 * Those portions are: Copyright (C) 2002 Manuel Novoa III
 */

#include "dryos.h"
//#include <errno.h>

/* from https://github.com/torvalds/linux/blob/master/vsprintf.c
 * Canon's vsnprintf returns the number of characters actually written,
 * so we were using snprintf like this, even on desktop tools...
 * see https://lwn.net/Articles/69419/ */

#undef vsnprintf

/**
 * vscnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The return value is the number of characters which have been written into
 * the @buf not including the trailing '\0'. If @size is == 0 the function
 * returns 0.
 *
 * If you're not already dealing with a va_list consider using scnprintf().
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	unsigned int i;

	i = vsnprintf(buf, size, fmt, args);

	if (likely(i < size))
		return i;
	if (size != 0)
		return size - 1;
	return 0;
}

/**
 * scnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The return value is the number of characters written into @buf not including
 * the trailing '\0'. If @size is == 0 the function returns 0.
 */

int scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}

// sometimes gcc likes very much the default fprintf and uses that one
// => renamed to my_fprintf to force it to use this one
int
my_fprintf(
    FILE *          file,
    const char *        fmt,
    ...
)
{
    va_list         ap;
    int len = 0;
    
    const int maxlen = 512;
    char buf[maxlen];

    va_start( ap, fmt );
    len = vscnprintf( buf, maxlen, fmt, ap );
    va_end( ap );
    FIO_WriteFile( file, buf, len );
    
    return len;
}

// Don't use strcmp since we don't have it
int
streq( const char * a, const char * b )
{
    while( *a && *b )
        if( *a++ != *b++ )
            return 0;
    return *a == *b;
}

int toupper(int c)
{
    if(('a' <= c) && (c <= 'z'))
        return 'A' + c - 'a';
    return c;
}

int tolower(int c)
{
    if(('A' <= c) && (c <= 'Z'))
        return 'a' + c - 'A';
    return c;
}

static int errno;
int* __errno(void) { return &errno; }

int islower(int x) { return ((x)>='a') && ((x)<='z'); }
int isupper(int x) { return ((x)>='A') && ((x)<='Z'); }
int isalpha(int x) { return islower(x) || isupper(x); }
int isdigit(int x) { return ((x)>='0') && ((x)<='9'); }
int isxdigit(int x) { return isdigit(x) || (((x)>='A') && ((x)<='F')) || (((x)>='a') && ((x)<='f')); }
int isalnum(int x) { return isalpha(x) || isdigit(x); }
int ispunct(int x) { return strchr("!\"#%&'();<=>?[\\]*+,-./:^_{|}~",x)!=0; }
int isgraph(int x) { return ispunct(x) || isalnum(x); }
int isspace(int x) { return strchr(" \r\n\t",x)!=0; }
int iscntrl(int x) { return strchr("\x07\x08\r\n\x0C\x0B\x09",x)!=0; }

/**
 * 5D3            cacheable     uncacheable
 * newlib memset: 237MB/s       100MB/s
 * memset64     : 194MB/s (!)   130MB/s
 */

void* FAST memset64(void* dest, int val, size_t n)
{
    size_t i = 0;
    if ((intptr_t)dest & 7)
    {
        dest = (void*)((intptr_t)dest & ~7) + 8;
        i++;
        n -= 8 - ((intptr_t)dest & 7);
    }
    uint64_t* dst = (uint64_t*) dest;
    uint64_t v1 = ((uint64_t) val) & 0xFFFFFFFFull;
    uint64_t v = v1 << 32 | v1;
    for(; i < n/8; i++)
        *dst++ = v;
    return (void*)dest;
}

/**
 * 5D3            cacheable     uncacheable
 * newlib memcpy: 75MB/s        17MB/s
 * diet memcpy  : 19MB/s        4MB/s
 * memcpy64     : 80MB/s        32MB/s
 */

void* FAST memcpy64(void* dest, void* srce, size_t n)
{
    size_t i = 0;
    if ((intptr_t)dest & 7)
    {
        srce = (void*)((intptr_t) srce & ~7) + 8;
        i++;
        n -= 8 - ((intptr_t)dest & 7);
    }
    if ((intptr_t)dest & 7)
    {
        dest = (void*)((intptr_t) dest & ~7) + 8;
    }
    uint64_t* dst = (uint64_t*) dest;
    uint64_t* src = (uint64_t*) srce;
    for(; i < n/8; i++)
        *dst++ = *src++;
    
    return (void*)dest;
}
