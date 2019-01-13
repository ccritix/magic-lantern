/* For use with dietlibc or other standards-compliant (v)snprintf */
/* Not to be used with DryOS vsnprintf. */

/* from https://github.com/torvalds/linux/blob/master/vsprintf.c
 * Canon's vsnprintf returns the number of characters actually written,
 * so we were using snprintf like this, even on desktop tools...
 * see https://lwn.net/Articles/69419/ */

#define unlikely(exp) __builtin_expect(exp,0)
#define likely(exp) __builtin_expect(exp,1)

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
static int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
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

static int scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(buf, size, fmt, args);
	va_end(args);

	return i;
}

/* since Canon's vsnprintf returns the number of chars actually written,
 * we were using the return value of snprintf/vsnprintf
 * as described in https://lwn.net/Articles/69419/ 
 * disallow this usage; we really meant scnprintf/vscnprintf
 * note: if we don't care about return value, they are the same as snprintf/vsnprintf
 */
#define snprintf (void)scnprintf
#define vsnprintf (void)vscnprintf
