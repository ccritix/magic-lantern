#ifdef CONFIG_MAGICLANTERN
#include <dryos.h>
#else
#include <stdio.h>
#include <string.h>
#endif
#include <linux/types.h>

/* much easier than taking it from stdio */
int putchar(int c)
{
    printf("%s", (char*) &c);
    return c;
}

void perror(const char * s)
{
    printf("Error: %s\n", s);
}

/* used to "Print DEVSLP information" - very unlikely to be called for CF cards */
int get_id_log_page_data(int fd, __u8 pagenr, __u8 *buf)
{
    printf("fixme: get_id_log_page_data\n");
    return 0;
}

