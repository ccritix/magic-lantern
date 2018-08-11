/**
 * Generic (not model-specific) init code called during ML startup.
 * To be included only in boot-*.c
 */

/* Called before Canon's init_task */
void boot_pre_init_task(void);

/* Called after Canon's init_task */
void boot_post_init_task(void);

/** Specified by the linker */
extern uint32_t _bss_start[], _bss_end[], _text_start[];

static int my_init_task(int a, int b, int c, int d)
{
    qprintf("[BOOT] autoexec.bin loaded at %X - %X.\n", &_text_start, &_bss_end);

    /* early initialization (before Canon's init_task) */
    qprintf("[BOOT] calling pre_init_task %X.\n", (uint32_t) &boot_pre_init_task);
    boot_pre_init_task();

    qprintf("[BOOT] starting init_task...\n");
    int ans = init_task(a,b,c,d);
    qprintf("[BOOT] init_task returned.\n");

    /* regular initialization (usually just launching new tasks) */
    /* we can't do much from here, as the memory reserved for this task is very limited */
    qprintf("[BOOT] calling post_init_task %X.\n", (uint32_t) &boot_post_init_task);
    boot_post_init_task();

    qprintf("[BOOT] my_init_task completed.\n");
    return ans;
}
