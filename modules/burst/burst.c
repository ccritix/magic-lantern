/**
 * Tweaks for burst (continuous) shooting.
 * 
 * (C) 2016 a1ex
 */

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <menu.h>
#include <config.h>
#include <lens.h>
#include <shoot.h>
#include <raw.h>
#include <bmp.h>
#include <powersave.h>
#include <fileprefix.h>

static CONFIG_INT("burst.max_count", burst_max_count, 0);
static CONFIG_INT("burst.display", burst_preview, 0);

static int check_preconditions()
{
    if (is_movie_mode()) return 0;
    if (!is_continuous_drive()) return 0;

    return 1;
}

/* last file number when the camera was idle (not taking pictures) */
static int last_idle_file_num = 0;

static void burst_file_num_poll()
{
    if (lens_info.job_state && get_halfshutter_pressed())
    {
        /* nothing to do, prop handler takes care of it */
    }
    else
    {
        last_idle_file_num = get_shooting_card()->file_number;
    }
}

static void display_off_if_unchanged(int unused, int preview_time)
{
    /* If our magic number is still on the screen,
     * the display was probably not updated by Canon code
     * => we may turn it off.
     */
    if (display_is_on() && bmp_getpixel(0, 0) == 0x77)
    {
        display_off();
    }
}

static void burst_preview_run()
{
    static int triggered = 0;

    if (lens_info.job_state && get_halfshutter_pressed() && !lv)
    {
        int pics_taken = MOD(get_shooting_card()->file_number - last_idle_file_num, 10000);

        /* if we took at least 2 pictures in a burst sequence, 
         * wait for display to turn off, then force it back on */
        if (pics_taken >= 2 && !display_is_on())
        {
            display_on();
            triggered = 1;
        }

        /* only if we have forced the display on, display a preview */
        if (triggered)
        {
            /* fixme: hack for tricking the raw backend to work during picture taking */
            int old_state = gui_state;
            gui_state = GUISTATE_QR_ZOOM;
            int raw_avail = raw_update_params();
            if (gui_state == GUISTATE_QR_ZOOM) gui_state = old_state;

            static int errors = 0;

            if (raw_avail)
            {
                errors = 0;

                /* preview the raw image and show some indicators on the screen */
                clrscr();
                bmp_printf(FONT_MED, 0, 0, "%s%04d\n", get_file_prefix(), get_shooting_card()->file_number);
                bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, "%d (%d)", burst_count, avail_shot);
                raw_preview_fast();

                /* place a signature on the screen, to know
                 * whether Canon code has overwritten it or not */
                bmp_putpixel(0, 0, 0x77);
                msleep(100);
            }
            else
            {
                /* ignore transient errors about raw data,
                 * and print only persistent ones */
                errors++;
                if (errors > 10)
                {
                    bmp_printf(FONT_MED, 0, 0, "Raw data not available.\n");
                }
                msleep(20);
            }
        }
    }
    else
    {
        if (triggered)
        {
            /* turn off the display if our preview is still there,
             * and stop previewing */
            delayed_call(2000, display_off_if_unchanged, 0);
            triggered = 0;
        }
    }
}

static void burst_max_count_run(int file_number)
{
    if (!check_preconditions())
    {
        return;
    }

    if (lens_info.job_state && get_halfshutter_pressed())
    {
        /* during picture taking? */
        int pics_taken = MOD(file_number - last_idle_file_num, 10000);

        if (pics_taken > burst_max_count)
        {
            /* release the shutter button, stopping the burst sequence */
            SW2(0,0);
            SW1(0,0);
        }
    }
}

/* Prop handlers are used to stop the burst sequence quickly,
 * even if other CPU-intensive tasks are running.
 *
 * The image preview is slower, so we run it from shoot_task
 * to avoid slowing down Canon code during a burst sequence.
 */

PROP_HANDLER(PROP_FILE_NUMBER_A)
{
    if (burst_max_count && *(get_shooting_card()->drive_letter) == 'A')
    {
        burst_max_count_run(buf[0]);
    }
}

PROP_HANDLER(PROP_FILE_NUMBER_B)
{
    if (burst_max_count && *(get_shooting_card()->drive_letter) == 'B')
    {
        burst_max_count_run(buf[0]);
    }
}

static unsigned int burst_shoot_cbr()
{
    if (!check_preconditions())
    {
        return 0;
    }

    burst_file_num_poll();

    if (burst_preview)
    {
        burst_preview_run();
    }

    return 0;
}

static MENU_UPDATE_FUNC(burst_update)
{
    if (!is_continuous_drive())
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "This feature requires continuous drive mode.");
    }
}

static MENU_UPDATE_FUNC(burst_preview_update)
{
    burst_update(entry, info);
    menu_checkdep_raw(entry, info);
}

static struct menu_entry burst_menu[] = {
    {
        .name       = "Burst mode tweaks",
        .select     = menu_open_submenu,
        .update     = burst_update,
        .depends_on = DEP_PHOTO_MODE,
        .children   = (struct menu_entry[]) {
            {
                .name       = "Burst preview",
                .priv       = &burst_preview,
                .max        = 1,
                .update     = burst_preview_update,
                .help       = "Show image previews while shooting a burst sequence.",
                .help2      = "Useful when taking pictures without using the viewfinder.",
            },
            {
                .name       = "Burst max count",
                .priv       = &burst_max_count, /* add 1 to get the desired count */
                .max        = 7,
                .update     = burst_update,
                .choices    = CHOICES("OFF", "2", "3", "4", "5", "6", "7", "8"),
                .help       = "Limit number of pictures in a burst sequence.",
            },
            MENU_EOL
        },
    },
};

static unsigned int burst_init()
{
    menu_add("Shoot", burst_menu, COUNT(burst_menu));

    return 0;
}

static unsigned int burst_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(burst_init)
    MODULE_DEINIT(burst_deinit)
MODULE_INFO_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_SHOOT_TASK, burst_shoot_cbr, 0)
MODULE_CBRS_END()

MODULE_PROPHANDLERS_START()
    MODULE_PROPHANDLER(PROP_FILE_NUMBER_A)
    MODULE_PROPHANDLER(PROP_FILE_NUMBER_B)
MODULE_PROPHANDLERS_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(burst_max_count)
    MODULE_CONFIG(burst_preview)
MODULE_CONFIGS_END()
