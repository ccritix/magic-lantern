
#define __rtc_sync_c__

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <beep.h>
#include <console.h>
#include <screenshot.h>

#include <math.h>
#include <float.h>
#include <string.h>

#include "../plot/plot.h"


/* number of microseconds we have to compensate every 60 seconds */
static int32_t us_correction_value = 0;
static int32_t us_correction_value_last = 0;

uint64_t rtc_get_us_clock_value()
{
    float timer_value = get_us_clock_value();
    int64_t correction = (timer_value / 60000000.0f) * us_correction_value;
    
    return timer_value + correction;
}

/* return the  */
uint32_t rtc_calibrate()
{
    /* determine the RTC read duration and approximate RTC second tick time */
    struct tm last_time;
    uint64_t rtc_tick_timestamp = 0;
    uint64_t last_rtc_read = rtc_get_us_clock_value();
    
    uint32_t int_state = cli();
    LoadCalendarFromRTC(&last_time);
    sei(int_state);
    
    console_printf("Profiling RTC read duration...\n");
    uint32_t loop = 0;
    while(1)
    {
        struct tm now;
        
        int_state = cli();
        LoadCalendarFromRTC(&now);
        sei(int_state);
        
        /* did RTC second field tick? */
        if(now.tm_sec != last_time.tm_sec)
        {
            console_printf("  second tick...\n");
            
            /* ok remember the microsecond timer value when the RTC time changed */
            rtc_tick_timestamp = last_rtc_read;
            
            /* do we have enough statistical data about RTC read delay? */
            if(loop > 50)
            {
                break;
            }
        }
        
        last_time = now;
        loop++;
        msleep(10);
        
        if(gui_menu_shown())
        {
            beep();
            NotifyBox(5000, "Cancelled due to menu activity");
            return 0;
        }
    }
   
    console_printf("Preparing for next tick...\n");
    /* now get close to the RTC tick time */
    while(1)
    {
        uint64_t us_clock = rtc_get_us_clock_value();
        uint64_t delta = (us_clock - rtc_tick_timestamp) % 1000000;
        
        /* okay 950 ms expired since last time the RTC second tick happened. stop sleeping and go over to active polling */
        if(delta > 950000)
        {
            break;
        }
        msleep(10);
        
        if(gui_menu_shown())
        {
            beep();
            NotifyBox(5000, "Cancelled due to menu activity");
            return 0;
        }
    }
    
    /* now block the whole tasks system with RTC reads until RTC second changes */
    console_printf("Detect exact tick time...\n");
    LoadCalendarFromRTC(&last_time);
    
    uint64_t rtc_tick_timestamp_exact = 0;
    
    /* fully block interrupts */
    int_state = cli();
    while(1)
    {
        struct tm now;
        uint64_t start = rtc_get_us_clock_value();
        
        LoadCalendarFromRTC(&now);
        
        if(now.tm_sec != last_time.tm_sec)
        {
            rtc_tick_timestamp_exact = start;
            break;
        }
        
        last_time = now;
        
        if(gui_menu_shown())
        {
            sei(int_state);
            beep();
            NotifyBox(5000, "Cancelled due to menu activity");
            return 0;
        }
    }
    sei(int_state);
    
    uint32_t offset = (uint32_t)(rtc_tick_timestamp_exact % 1000000);
    
    console_printf("  RTC offset: %d us offset to system boot time\n", offset);
    
    return offset;
}

int32_t rtc_sync_get_drift()
{
    plot_coll_t *coll_drift = plot_alloc_data(1);
    plot_coll_t *coll_drift_avg = plot_alloc_data(1);
    plot_graph_t *plot = plot_alloc_graph(20, 60, 680, 200);
    plot_graph_t *plot_avg = plot_alloc_graph(20, 60, 680, 200);
    
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    
    /* measured values are a bit darker */
    plot->dot_size = 1;
    plot->color_dots = COLOR_GRAY(40);
    plot->color_lines = COLOR_GRAY(40);
    
    /* average overlay plot transparent */
    plot_avg->color_range = PLOT_COLOR_NONE;
    plot_avg->color_axis = PLOT_COLOR_NONE;
    plot_avg->color_border = PLOT_COLOR_NONE;
    plot_avg->color_bg = PLOT_COLOR_NONE;
    
    /* determine the RTC read duration and approximate RTC second tick time */
    int64_t delta_sums = 0;
    uint32_t deltas_sampled = 0;
    uint32_t last_offset = 0;

    bmp_printf(FONT_LARGE, 10, 20, " -> Waiting for first minute...    ");
    
    while(1)
    {
        struct tm now;
        
        LoadCalendarFromRTC(&now);

        /* get range */
        float avg = plot_get_average(coll_drift, 0);
        float delta_low = 0;
        float delta_high = 0;
        plot_get_extremes(coll_drift, 0, PLOT_MIN, PLOT_MAX, &delta_low, &delta_high);
        
        float range = delta_high - delta_low;
        plot_set_range(plot, 0, coll_drift->used, delta_low - range * 0.01, delta_high + range * 0.01);
        plot_set_range(plot_avg, 0, coll_drift_avg->used, delta_low - range * 0.01, delta_high + range * 0.01);

        /* plot graph */
        plot_graph_update(coll_drift, plot);
        plot_graph_update(coll_drift_avg, plot_avg);
        
        /* will RTC minute field tick? */
        if(now.tm_sec == 59)
        {
            bmp_fill(COLOR_BLACK, 0, 0, 720, 58);
            bmp_printf(FONT_LARGE, 10, 20, " -> Calibration...    ");

            /* do a calibration run starting at this second as the RTC does a offset correction every minute */
            uint32_t offset = rtc_calibrate();
            
            if(last_offset)
            {
                int32_t delta = (int32_t)offset - (int32_t)last_offset;
                
                delta_sums += delta;
                deltas_sampled++;
                
                plot_add(coll_drift, (float)delta);
                plot_add(coll_drift_avg, (float)plot_get_average(coll_drift, 0));
                
                bmp_fill(COLOR_BLACK, 0, 260, 720, 480);
                bmp_printf(FONT_LARGE, 10, 260, " Delta: %d  (%d - %d) Avg: %d   ", delta, offset, last_offset, (int32_t)avg);
            }
            
            last_offset = offset;
            
            /* after 45 minutes we are done */
            if(deltas_sampled > 45)
            {
                break;
            }
        }
        
        bmp_fill(COLOR_BLACK, 0, 0, 720, 58);
        bmp_printf(FONT_LARGE, 10, 20, " -> Waiting for %s minute...  (%d)  ", last_offset?"next":"first", now.tm_sec);
        
        if(gui_menu_shown())
        {
            beep();
            NotifyBox(5000, "Cancelled due to menu activity");
            return 0;
        }
        
        msleep(100);
    }
    
    take_screenshot(SCREENSHOT_FILENAME_AUTO, SCREENSHOT_BMP);
    return (int32_t)(delta_sums / deltas_sampled);
}


static MENU_SELECT_FUNC(rtc_sync_cold_calib)
{
    gui_stop_menu();
    msleep(500);
    bmp_printf(FONT_LARGE, 10, 20, " -> Starting RTC Sync operation...");
    msleep(500);
    
    /* start a cold calibration */
    us_correction_value = 0;
    us_correction_value_last = rtc_sync_get_drift();
    us_correction_value -= us_correction_value_last;
    
    beep();
}

static MENU_SELECT_FUNC(rtc_sync_warm_calib)
{
    gui_stop_menu();
    msleep(500);
    bmp_printf(FONT_LARGE, 10, 20, " -> Starting RTC Sync operation...");
    msleep(500);
    
    /* now rerun the calibration and check remaining delta */
    us_correction_value_last = rtc_sync_get_drift();
    us_correction_value -= us_correction_value_last;
    
    beep();
}
    
static MENU_SELECT_FUNC(rtc_sync_endless_calib)
{
    gui_stop_menu();
    msleep(500);
    bmp_printf(FONT_LARGE, 10, 20, " -> Starting RTC Sync operation...");
    msleep(500);
    
    plot_coll_t *coll = plot_alloc_data(1);
    plot_graph_t *plot = plot_alloc_graph(20, 20, 680, 200);
    
    uint32_t loops = 0;
    while(1)
    {
        uint32_t offset = rtc_calibrate();
        plot_add(coll, (float)offset);
        
        //plot_autorange(coll, plot);
        float delta_low = 0;
        float delta_high = 0;
        float avg = plot_get_average(coll, 0);
        plot_get_extremes(coll, 0, avg * 0.8, avg * 1.2, &delta_low, &delta_high);
        
        /* plot graph */
        plot_set_range(plot, 0, coll->used, delta_low * 1, delta_high * 1);
        
        plot_graph_update(coll, plot);
        
        if(gui_menu_shown())
        {
            beep();
            NotifyBox(5000, "Cancelled due to menu activity");
            break;
        }
        
        loops++;
    }
}

static struct menu_entry rtc_sync_menu[] =
{
    {
        .name = "RTC sync",
        .help = "RTC synchronization tests.",
        .submenu_width = 710,
        .children = (struct menu_entry[])
        {
            {
                .name = "Cold calibration",
                .priv = &rtc_sync_cold_calib,
                .select = (void(*)(void*,int))&run_in_separate_task,
                .help = "Run a clean calibration run (45 min)",
            },
            {
                .name = "Rerun calibration",
                .priv = &rtc_sync_warm_calib,
                .select = (void(*)(void*,int))&run_in_separate_task,
                .help = "Re-run calibration run (45 min)",
            },
            {
                .name = "Plot clock deviation",
                .priv = &rtc_sync_endless_calib,
                .select = (void(*)(void*,int))&run_in_separate_task,
                .help = "Permanently shows clock deviation in a plot",
            },
            MENU_EOL,
        },
    },
};

static unsigned int rtc_sync_init()
{
    menu_add("Debug", rtc_sync_menu, COUNT(rtc_sync_menu));
    return 0;
}

static unsigned int rtc_sync_deinit()
{
    return 0;
}


MODULE_INFO_START()
    MODULE_INIT(rtc_sync_init)
    MODULE_DEINIT(rtc_sync_deinit)
MODULE_INFO_END()

