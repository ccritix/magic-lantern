/**
 * Auto ETTR (Expose To The Right).
 * 
 * Optimize the exposure for raw shooting (photo + video).
 */

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>
#include <raw.h>
#include <lens.h>
#include <math.h>

static CONFIG_INT("auto.ettr", auto_ettr, 0);
static CONFIG_INT("auto.ettr.trigger", auto_ettr_trigger, 2);
static CONFIG_INT("auto.ettr.ignore", auto_ettr_ignore, 2);
static CONFIG_INT("auto.ettr.level", auto_ettr_target_level, 0);
static CONFIG_INT("auto.ettr.max.tv", auto_ettr_max_shutter, 88);
static CONFIG_INT("auto.ettr.clip", auto_ettr_clip, 0);
static CONFIG_INT("auto.ettr.mode", auto_ettr_adjust_mode, 0);

#define AUTO_ETTR_TRIGGER_ALWAYS_ON (auto_ettr_trigger == 0 || is_intervalometer_running())
#define AUTO_ETTR_TRIGGER_AUTO_SNAP (auto_ettr_trigger == 1)
#define AUTO_ETTR_TRIGGER_PHOTO (AUTO_ETTR_TRIGGER_ALWAYS_ON || AUTO_ETTR_TRIGGER_AUTO_SNAP)
#define AUTO_ETTR_TRIGGER_BY_SET (auto_ettr_trigger == 2)
#define AUTO_ETTR_TRIGGER_BY_HALFSHUTTER_DBLCLICK (auto_ettr_trigger == 3)

extern int hdr_enabled;
#define HDR_ENABLED hdr_enabled

/* also used for display on histogram */
static int auto_ettr_get_correction()
{
    static int last_value = INT_MIN;
    
    /* this is kinda slow, don't run it very often */
    static int aux = INT_MIN;
    if (!lv && !should_run_polling_action(100, &aux) && last_value != INT_MIN)
        return last_value;
    
    int gray_proj = 
        auto_ettr_clip == 0 ? GRAY_PROJECTION_MAX_RGB :
        auto_ettr_clip == 1 ? GRAY_PROJECTION_MAX_RB :
        auto_ettr_clip == 2 ? GRAY_PROJECTION_MEDIAN_RGB : -1;
    
    /* compute the raw levels for more percentile values; will help if the image is overexposed */
    /* if it's not, we'll use only the first value (the one from menu) */
    int percentiles[12] = {(1000 - auto_ettr_ignore), 950, 900, 800, 750, 700, 600, 500, 300, 200, 150, 100};

    int raw_values[COUNT(percentiles)];
    static float diff_from_lower_percentiles[COUNT(percentiles)-1] = {0};

    int ok = raw_hist_get_percentile_levels(percentiles, raw_values, COUNT(percentiles), gray_proj);
    
    if (ok != 1)
    {
        last_value = INT_MIN;
        return last_value;
    }
    
    float ev = raw_to_ev(raw_values[0]);
    
    //~ bmp_printf(FONT_MED, 50, 200, "%d ", MEMX(0xc0f08030));
    float target = MIN(auto_ettr_target_level, -0.5);
    float correction = target - ev;
    if (ev < -0.1)
    {
        /* cool, we know exactly how much to correct, we'll return "correction" */
        
        /* save data for helping with future overexposed shots */
        for (int k = 0; k < COUNT(percentiles)-1; k++)
            diff_from_lower_percentiles[k] = ev - raw_to_ev(raw_values[k+1]);
        
        //~ bmp_printf(FONT_MED, 0, 100, "overexposure hints: %d %d %d\n", (int)(diff_from_lower_percentiles[0] * 100), (int)(diff_from_lower_percentiles[1] * 100), (int)(diff_from_lower_percentiles[2] * 100));
    }
    else
    {
        /* image is overexposed */
        /* and we don't know how much to go back in order to fix the overexposure */

        /* from the previous shot, we know where the highlights were, compared to some lower percentiles */
        /* let's assume this didn't change; meter at those percentiles and extrapolate the result */

        int num = 0;
        float sum = 0;
        float min = 100000;
        float max = -100000;
        for (int k = 0; k < COUNT(percentiles)-1; k++)
        {
            if (diff_from_lower_percentiles[k] > 0)
            {
                float lower_ev = raw_to_ev(raw_values[k+1]);
                if (lower_ev < -0.1)
                {
                    /* if the scene didn't change, we should be spot on */
                    /* don't update the correction hints, since we don't know exactly where we are */
                    ev = lower_ev + diff_from_lower_percentiles[k];
                    
                    /* we need to get a stronger correction than with the overexposed metering */
                    /* otherwise, the scene probably changed */
                    if (target - ev < correction)
                    {
                        float corr = target - ev;
                        min = MIN(min, corr);
                        max = MAX(max, corr);
                        
                        /* first estimations are more reliable, weight them a bit more */
                        sum += corr * (COUNT(percentiles) - k);
                        num += (COUNT(percentiles) - k);
                        //~ msleep(500);
                        //~ bmp_printf(FONT_MED, 0, 100+20*k, "overexposure fix: k=%d diff=%d ev=%d corr=%d\n", k, (int)(diff_from_lower_percentiles[k] * 100), (int)(ev * 100), (int)(corr * 100));
                    }
                }
            }
        }
        
        float mean = sum/num;
        if (num < 3 || max - mean > 1 || mean - min > 1)
        {
            /* scene changed? measurements from previous shot not confirmed or vary too much?
             * 
             * we'll use a heuristic: for 1% of blown out image, go back 1EV, for 100% go back 10EV */
            float overexposed = raw_hist_get_overexposure_percentage(GRAY_PROJECTION_AVERAGE_RGB) / 100.0;
            //~ bmp_printf(FONT_MED, 0, 80, "overexposure area: %d/100%%\n", (int)(overexposed * 100));
            //~ bmp_printf(FONT_MED, 0, 120, "fail info: (%d %d %d %d) (%d %d %d)", raw_values[0], raw_values[1], raw_values[2], raw_values[3], (int)(diff_from_lower_percentiles[0] * 100), (int)(diff_from_lower_percentiles[1] * 100), (int)(diff_from_lower_percentiles[2] * 100));
            float corr = correction - log2f(1 + overexposed);
            int weight = MAX(num, 2);
            sum += corr * weight;
            num += weight;
        }

        /* use the average value for correction */
        correction = sum / num;
    }
    
    last_value = (int)(correction * 100);
    return last_value;
}

int auto_ettr_export_correction(int* out)
{
    int value = auto_ettr_get_correction();
    if (value == INT_MIN) return -1;
    *out = value;
    return 1;
}

/* returns: 0 = nothing changed, 1 = OK, -1 = exposure limits reached */
static int auto_ettr_work_m(int corr)
{
    int tv = lens_info.raw_shutter;
    int iso = lens_info.raw_iso;
    if (!tv || !iso) return 0;
    int old_expo = tv - iso;

    int delta = -corr * 8 / 100;

    static int prev_tv = 0;
    if (auto_ettr_adjust_mode == 1)
    {
        if (prev_tv != tv)
        {
            auto_ettr_max_shutter = tv;
            if (lv)
            {
                NotifyBox(2000, "Auto ETTR: Tv <= %s ", lens_format_shutter(tv));
                prev_tv = tv;
                return 0; /* wait for next iteration */
            }
            else
            {
                msleep(1000);
                bmp_printf(FONT_MED, 0, os.y0, "Auto ETTR: Tv <= %s ", lens_format_shutter(tv));
            }
        }
    }
    else
    {
        if (lv && prev_tv != tv && AUTO_ETTR_TRIGGER_ALWAYS_ON)
        {
            prev_tv = tv;
            return 0; /* small pause when you change exposure manually */
        }
    }

    int shutter_lim = auto_ettr_max_shutter;

    /* can't go slower than 1/fps in movie mode */
    if (is_movie_mode()) shutter_lim = MAX(shutter_lim, shutter_ms_to_raw(1000 / video_mode_fps));

    /* apply exposure correction */
    tv += delta;

    /* use the lowest ISO for which we can get shutter = shutter_lim or higher */
    int offset = MIN(tv - shutter_lim, iso - MIN_ISO);
    tv -= offset;
    iso -= offset;

    /* some shutter values are not accepted by Canon firmware */
    int tvr = round_shutter(tv, shutter_lim);
    iso += tvr - tv;
    
    /* analog iso can be only in 1 EV increments */
    int max_auto_iso = auto_iso_range & 0xFF;
    int isor = COERCE((iso + 4) / 8 * 8, MIN_ISO, max_auto_iso);
    
    /* cancel ISO rounding errors by adjusting shutter, which goes in smaller increments */
    /* this may choose a shutter speed higher than selected one, at high iso, which may not be desirable */
    tvr += isor - iso;
    int tv0 = tvr;
    tvr = round_shutter(tvr, shutter_lim);

    /* apply the new settings */
    int oks = hdr_set_rawshutter(tvr);
    int oki = hdr_set_rawiso(isor);

    /* don't let expo lock undo our changes */
    expo_lock_update_value();
    
    prev_tv = lens_info.raw_shutter;

    int new_expo = lens_info.raw_shutter - lens_info.raw_iso;
    
    if (ABS(new_expo - old_expo) >= 3) /* something changed? consider it OK, better than nothing */
        return 1;
    
    if (tvr > tv0 + 4) /* still underexposed? */
        return -1;
    
    return oks && oki ? 1 : -1;
}

static int auto_ettr_work_auto(int corr)
{
    int ae = lens_info.ae;
    int ae0 = ae;

    int delta = -corr * 8 / 100;

    /* apply exposure correction */
    ae = round_expo_comp(ae - delta);

    /* apply the new settings */
    int ok = hdr_set_ae(ae);

    if (ABS(lens_info.ae - ae0) >= 3) /* something changed? consider it OK, better than nothing */
        return 1;

    return ok ? 1 : -1;
}

static int auto_ettr_work(int corr)
{
    if (shooting_mode == SHOOTMODE_AV || shooting_mode == SHOOTMODE_TV || shooting_mode == SHOOTMODE_P)
        return auto_ettr_work_auto(corr);
    else
        return auto_ettr_work_m(corr);
}

static volatile int auto_ettr_running = 0;
static volatile int ettr_pics_took = 0;

static void auto_ettr_step_task(int corr)
{
    lens_wait_readytotakepic(64);
    int status = auto_ettr_work(corr);
    
    if (corr >= -45 && corr <= 70)
    {
        /* cool, we got the ideal exposure */
        beep();
        ettr_pics_took = 0;
    }
    else if (ettr_pics_took >= 3)
    {
        /* I give up */
        beep_times(3);
        ettr_pics_took = 0;
        msleep(1000);
        bmp_printf(FONT_MED, 0, os.y0, "Auto ETTR: giving up");
    }
    else if (status == -1)
    {
        beep_times(3);
        ettr_pics_took = 0;
        msleep(1000);
        bmp_printf(FONT_MED, 0, os.y0, "Auto ETTR: expo limits reached");
    }
    else if (AUTO_ETTR_TRIGGER_AUTO_SNAP)
    {
        /* take another pic */
        auto_ettr_running = 0;
        lens_take_picture(0, AF_DISABLE);
        ettr_pics_took++;
    }
    else if (AUTO_ETTR_TRIGGER_ALWAYS_ON)
    {
        beep_times(2);
    }
    auto_ettr_running = 0;
}

static void auto_ettr_step()
{
    if (!auto_ettr) return;
    if (shooting_mode != SHOOTMODE_M && shooting_mode != SHOOTMODE_AV && shooting_mode != SHOOTMODE_TV && shooting_mode != SHOOTMODE_P && shooting_mode != SHOOTMODE_MOVIE) return;
    int is_m = (shooting_mode == SHOOTMODE_M || shooting_mode == SHOOTMODE_MOVIE);
    if (lens_info.raw_iso == 0 && is_m) return;
    if (lens_info.raw_shutter == 0 && is_m) return;
    if (auto_ettr_running) return;
    if (HDR_ENABLED) return;
    int corr = auto_ettr_get_correction();
    if (corr != INT_MIN)
    {
        /* we'd better not change expo settings from prop task (we won't get correct confirmations) */
        auto_ettr_running = 1;
        task_create("ettr_task", 0x1c, 0x1000, auto_ettr_step_task, (void*) corr);
    }
}

static int auto_ettr_check_pre_lv()
{
    if (!auto_ettr) return 0;
    if (shooting_mode != SHOOTMODE_M && shooting_mode != SHOOTMODE_AV && shooting_mode != SHOOTMODE_TV && shooting_mode != SHOOTMODE_P && shooting_mode != SHOOTMODE_MOVIE) return 0;
    int is_m = (shooting_mode == SHOOTMODE_M || shooting_mode == SHOOTMODE_MOVIE);
    if (lens_info.raw_iso == 0 && is_m) return 0;
    if (lens_info.raw_shutter == 0 && is_m) return 0;
    if (HDR_ENABLED) return 0;
    int raw = is_movie_mode() ? raw_lv_is_enabled() : pic_quality & 0x60000;
    return raw;
}

static int auto_ettr_check_in_lv()
{
    if (!expsim) return 0;
    if (lv_dispsize != 1) return 0;
    if (LV_PAUSED) return 0;
    if (!liveview_display_idle()) return 0;
    return 1;
}

static int auto_ettr_check_lv()
{
    if (!auto_ettr_check_pre_lv()) return 0;
    if (!auto_ettr_check_in_lv()) return 0;
    return 1;
}

static volatile int auto_ettr_vsync_active = 0;
static volatile int auto_ettr_vsync_delta = 0;
static volatile int auto_ettr_vsync_counter = 0;

/* instead of changing settings via properties, we can override them very quickly */
static unsigned int auto_ettr_vsync_cbr(unsigned int ctx)
{
    if (auto_ettr_vsync_active)
    {
        int delta = auto_ettr_vsync_delta;
        int current_iso = get_frame_iso();
        int current_shutter = get_frame_shutter_timer();
        int altered_iso = current_iso;
        int altered_shutter = current_shutter;

        int max_shutter = get_max_shutter_timer();
        if (current_shutter > max_shutter) max_shutter = current_shutter;

        /* first increase shutter speed, since it gives the cleanest signal */
        while (delta > 0 && altered_shutter * 2 <= max_shutter)
        {
            altered_shutter *= 2;
            delta -= 8;
        }

        int max_iso = get_max_analog_iso();

        /* then try to increase ISO if we need more */
        while (delta > 0 && altered_iso + 8 <= max_iso)
        {
            altered_iso += 8;
            delta -= 8;
        }

        /* then try to decrease ISO until ISO 100, raw 72 (even with HTP, FRAME_ISO goes to 100) */
        while (delta < -8 && altered_iso - 8 >= 72)
        {
            altered_iso -= 8;
            delta += 8;
        }
        
        /* commit iso */
        set_frame_iso(altered_iso);

        /* adjust shutter with the remaining delta */
        altered_shutter = COERCE((int)roundf(powf(2, delta/8.0) * (float)altered_shutter), 2, max_shutter);
        
        /* commit shutter */
        set_frame_shutter_timer(altered_shutter);
        
        //~ bmp_printf(FONT_MED, 50, 70, "delta %d iso %d->%d shutter %d->%d max %d ",  auto_ettr_vsync_delta, current_iso, altered_iso, current_shutter, altered_shutter, get_max_shutter_timer());
        auto_ettr_vsync_counter++;
        return 1;
    }
    return 0;
}

static int auto_ettr_wait_lv_frames(int num_frames)
{
    auto_ettr_vsync_counter = 0;
    int count = 0;
    while (auto_ettr_vsync_counter < num_frames)
    {
        msleep(20);
        count++;
        if (count > num_frames * 10)
        {
            beep();
            NotifyBox(2000, "Vsync err");
            auto_ettr_vsync_delta = 0;
            return 0;
        }
    }
    return 1;
}

static void auto_ettr_on_request_task_fast()
{
    beep();
    
    int was_in_lv = lv;
    if (!lv) force_liveview();
    if (!lv) goto end;
    if (lv_dispsize != 1) set_lv_zoom(1);
    if (!auto_ettr_check_lv()) goto end;
    if (get_halfshutter_pressed())
    {
        msleep(500);
        if (get_halfshutter_pressed()) goto end;
    }

#undef AUTO_ETTR_DEBUG
#ifdef AUTO_ETTR_DEBUG
    auto_ettr_vsync_active = 1;
    int raw0 = raw_hist_get_percentile_level(500, GRAY_PROJECTION_GREEN);
    float ev0 = raw_to_ev(raw0);
    int y0 = 100 - ev0 * 20;
    for (int i = 0; i < 100; i++)
    {
        int delta = rand() % 160 - 80;
        auto_ettr_vsync_delta = delta;
        if (!auto_ettr_wait_lv_frames(2)) break;
        
        int raw = raw_hist_get_percentile_level(500, GRAY_PROJECTION_GREEN);
        float ev = raw_to_ev(raw);
        int x = 360 + delta * 3;
        int y = 100 - ev * 24; /* multiplier must be 8 x the one from delta */
        dot(x-16, y-16, COLOR_BLUE, 3);
        draw_angled_line(360, y0, 300, 1800-450, COLOR_RED);
        draw_angled_line(360, y0, 300, -450, COLOR_RED);
        draw_angled_line(0, 100, 720, 0, COLOR_RED);
    }
    auto_ettr_vsync_delta = 0;
    auto_ettr_wait_lv_frames(100);
#endif


    NotifyBox(100000, "Auto ETTR...");
    auto_ettr_vsync_active = 1;
    auto_ettr_vsync_delta = 0;
    raw_lv_request();
    
    for (int i = 0; i < 3; i++)
    {
        for (int k = 0; k < 5; k++)
        {
            /* see how far we are from the ideal exposure */
            int corr = auto_ettr_get_correction();
            if (corr == INT_MIN) break;
            
            /* override the liveview parameters via auto_ettr_vsync_cbr (much faster than via properties) */
            auto_ettr_vsync_delta += corr * 8 / 100;

            /* I'm confident the last iteration was accurate */
            if (corr >= -20 && corr <= 200)
                break;
            
            /* wait for 2 frames before trying again */
            if (!auto_ettr_wait_lv_frames(2)) goto end;
        }

        /* apply the correction via properties */
        if (auto_ettr_vsync_delta)
        {
            int corr = auto_ettr_vsync_delta * 100 / 8;
            auto_ettr_vsync_delta = 0;
            auto_ettr_work(corr);
        
            if (ABS(corr) <= 200)
            {
                /* looks like it settled */
                break;
            }
            else
            {
                if (i < 3)
                {
                    /* here we go again... */
                    msleep(1000);
                }
                else
                {
                    /* or... not? */
                    beep();
                    NotifyBox(2000, "Whoops");
                    goto end;
                }
            }
        }
    }

    NotifyBoxHide();

end:
    beep();
    auto_ettr_running = 0;
    auto_ettr_vsync_active = 0;
    raw_lv_release();

    if (lv && !was_in_lv) { msleep(200); close_liveview(); }
}

static void auto_ettr_step_lv_fast()
{
    if (!auto_ettr || !AUTO_ETTR_TRIGGER_ALWAYS_ON)
        return;
    
    if (!auto_ettr_check_lv())
        return;
    
    if (get_halfshutter_pressed())
        return;

    /* only poll exposure once per second */
    static int aux = INT_MIN;
    if (!should_run_polling_action(1000, &aux))
        return;
    
    raw_lv_request();
    int corr = auto_ettr_get_correction();
    
    /* only correct if the image is overexposed by more than 0.2 EV or underexposed by more than 1 EV */
    if (corr != INT_MIN && (corr < -20 || corr > 100))
    {
        auto_ettr_vsync_active = 1;
        auto_ettr_vsync_delta = 0;
        int k;
        for (k = 0; k < 5; k++)
        {
            /* see how far we are from the ideal exposure */
            if (k > 0) corr = auto_ettr_get_correction();
            if (corr == INT_MIN) break;
            
            /* override the liveview parameters via auto_ettr_vsync_cbr (much faster than via properties) */
            auto_ettr_vsync_delta += corr * 8 / 100;

            /* I'm confident the last iteration was accurate */
            if (corr >= -20 && corr <= 200)
                break;
            
            /* wait for 2 frames before trying again */
            if (!auto_ettr_wait_lv_frames(2)) break;
        }
        auto_ettr_vsync_active = 0;

        /* apply the final correction via properties */
        auto_ettr_work(auto_ettr_vsync_delta * 100 / 8);

        msleep(1000);
    }
    raw_lv_release();
}

static void auto_ettr_on_request_task_slow()
{
    beep();
    
    int was_in_lv = lv;
    if (!lv) force_liveview();
    if (!lv) goto end;
    if (lv_dispsize != 1) set_lv_zoom(1);
    if (!auto_ettr_check_lv()) goto end;
    if (get_halfshutter_pressed())
    {
        msleep(500);
        if (get_halfshutter_pressed()) goto end;
    }

    NotifyBox(100000, "Auto ETTR...");
    for (int k = 0; k < 5; k++)
    {
        msleep(500);
        
        raw_lv_request();
        int corr = auto_ettr_get_correction();
        raw_lv_release();

        if (corr != INT_MIN)
        {
            auto_ettr_work(corr);
            msleep(1000);
        }
        else
            break;
        
        if (corr >= -20 && corr <= 200) /* I'm confident the last iteration was accurate */
            break;
        
        if (get_halfshutter_pressed())
            break;
        
        if (k == 4)
        {
            beep();
            NotifyBox(2000, "Whoops");
            goto end;
        }
    }
    NotifyBoxHide();

end:
    beep();
    if (lv && !was_in_lv) close_liveview();
    auto_ettr_running = 0;
}

static void auto_ettr_step_lv_slow()
{
    if (!auto_ettr || !AUTO_ETTR_TRIGGER_ALWAYS_ON)
        return;
    
    if (!auto_ettr_check_lv())
        return;
    
    if (get_halfshutter_pressed())
        return;

    /* only update once per 1.5 seconds, so the exposure has a chance to be updated on the LCD */
    static int aux = INT_MIN;
    if (!should_run_polling_action(1500, &aux))
        return;
    
    raw_lv_request();
    int corr = auto_ettr_get_correction();
    raw_lv_release();
    
    /* only correct if the image is overexposed by more than 0.2 EV or underexposed by more than 1 EV */
    static int settled = 0;
    if (corr != INT_MIN && (corr < -20 || corr > 100))
    {
        auto_ettr_work(corr);
        settled = 0;
    }
    else
    {
        settled++;
        if (settled == 2) beep();
    }
}

static void auto_ettr_step_lv()
{
    if (can_set_frame_iso() && can_set_frame_shutter_timer())
        auto_ettr_step_lv_fast();
    else
        auto_ettr_step_lv_slow();
}

static void auto_ettr_on_request_task(int unused)
{
    if (can_set_frame_iso() && can_set_frame_shutter_timer())
        auto_ettr_on_request_task_fast();
    else
        auto_ettr_on_request_task_slow();
}

static unsigned int auto_ettr_keypress_cbr(unsigned int key)
{
    if (!auto_ettr) return 1;
    if (AUTO_ETTR_TRIGGER_PHOTO) return 1;
    if (!display_idle()) return 1;
    if (!auto_ettr_check_pre_lv()) return 1;
    if (lv && !auto_ettr_check_in_lv()) return 1;
    
    if (
            (AUTO_ETTR_TRIGGER_BY_SET && key == MODULE_KEY_PRESS_SET) ||
            (AUTO_ETTR_TRIGGER_BY_HALFSHUTTER_DBLCLICK && detect_double_click(key, MODULE_KEY_PRESS_HALFSHUTTER, MODULE_KEY_UNPRESS_HALFSHUTTER)) ||
       0)
    {
        if (!auto_ettr_running)
        {
            auto_ettr_running = 1;
            task_create("ettr_task", 0x1c, 0x1000, auto_ettr_on_request_task, (void*) 0);
            if (AUTO_ETTR_TRIGGER_BY_SET) return 0;
        }
    }
    return 1;
}

static MENU_UPDATE_FUNC(auto_ettr_update)
{
    if (shooting_mode != SHOOTMODE_M && shooting_mode != SHOOTMODE_AV && shooting_mode != SHOOTMODE_TV && shooting_mode != SHOOTMODE_P && shooting_mode != SHOOTMODE_MOVIE)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Auto ETTR only works in M, Av, Tv, P and RAW MOVIE modes.");

    int is_m = (shooting_mode == SHOOTMODE_M || shooting_mode == SHOOTMODE_MOVIE);
    if (lens_info.raw_iso == 0 && is_m)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Auto ETTR requires manual ISO.");

    int raw = is_movie_mode() ? raw_lv_is_enabled() : pic_quality & 0x60000;

    if (!raw)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "[%s] You must shoot RAW in order to use this.", is_movie_mode() ? "MOVIE" : "PHOTO");

    if (!lv && !can_use_raw_overlays_photo() && AUTO_ETTR_TRIGGER_PHOTO)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Photo RAW data not available, try in LiveView.");

    if (image_review_time == 0 && AUTO_ETTR_TRIGGER_PHOTO)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Enable image review from Canon menu.");

    if (HDR_ENABLED)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Not compatible with HDR bracketing.");

    if (lv && !expsim)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "In LiveView, this requires ExpSim enabled.");
    
    if (is_continuous_drive() && AUTO_ETTR_TRIGGER_PHOTO)
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Not fully compatible with continuous drive.");
    
    if (auto_ettr)
        MENU_SET_RINFO("%dEV/%d.%d%%", auto_ettr_target_level, auto_ettr_ignore/10, auto_ettr_ignore%10);

    if (auto_ettr)
        MENU_SET_VALUE(
            AUTO_ETTR_TRIGGER_ALWAYS_ON ? "Always ON" : 
            AUTO_ETTR_TRIGGER_AUTO_SNAP ? "Auto Snap" : 
            AUTO_ETTR_TRIGGER_BY_SET ? "Press SET" : 
            AUTO_ETTR_TRIGGER_BY_HALFSHUTTER_DBLCLICK ? "HalfS DBC" : "err"
        );

    if (!AUTO_ETTR_TRIGGER_PHOTO)
        MENU_SET_HELP("Press the shortcut key to optimize the exposure (ETTR).");
    else if (AUTO_ETTR_TRIGGER_ALWAYS_ON)
    {
        if (lv) MENU_SET_HELP("In LiveView, just wait for exposure to settle, then shoot.");
        else MENU_SET_HELP("Take a test picture (underexposed). Next pic will be ETTR.");
    }
    else if (AUTO_ETTR_TRIGGER_AUTO_SNAP)
        MENU_SET_HELP("Press shutter once. ML will take a pic and retry if needed.");
}

static MENU_UPDATE_FUNC(auto_ettr_max_shutter_update)
{
    MENU_SET_VALUE("%s", lens_format_shutter(auto_ettr_max_shutter));
    if (auto_ettr_adjust_mode == 1)
        MENU_SET_WARNING(MENU_WARN_INFO, "Adjust shutter speed from top scrollwheel, outside menu.");
}

static MENU_SELECT_FUNC(auto_ettr_max_shutter_toggle)
{
    if (auto_ettr_adjust_mode == 0)
        auto_ettr_max_shutter = mod(auto_ettr_max_shutter/4*4 - 16 + delta * 4, 152 - 16 + 4) + 16;
}

PROP_HANDLER(PROP_GUI_STATE)
{
    int* data = buf;
    if (data[0] == GUISTATE_QR)
    {
        if (AUTO_ETTR_TRIGGER_PHOTO)
            auto_ettr_step();
    }
}

MENU_UPDATE_FUNC(auto_ettr_intervalometer_warning)
{
    if (auto_ettr && image_review_time == 0 && AUTO_ETTR_TRIGGER_PHOTO)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Auto ETTR: enable image review from Canon menu.");
}

void auto_ettr_intervalometer_wait()
{
    if (auto_ettr && image_review_time)
    {
        /* make sure auto ETTR has a chance to run (it's triggered by prop handler on QR mode) */
        /* timeout: a bit more than exposure time, to handle long expo noise reduction */
        for (int i = 0; i < raw2shutter_ms(lens_info.raw_shutter)/100; i++)
        {
            if (gui_state == GUISTATE_PLAYMENU || gui_state == GUISTATE_QR) break;
            msleep(150);
        }
        msleep(500);
    }
}

static unsigned int auto_ettr_polling_cbr()
{
    if (lv && !recording)
        auto_ettr_step_lv();
    return 0;
}

static struct menu_entry ettr_menu[] =
{
    {
        .name = "Auto ETTR", 
        .priv = &auto_ettr, 
        .update = auto_ettr_update,
        .max = 1,
        .help  = "Auto expose to the right when you shoot RAW.",
        .submenu_width = 710,
        .children =  (struct menu_entry[]) {
            {
                .name = "Trigger mode",
                .priv = &auto_ettr_trigger,
                .max = 3,
                .choices = CHOICES("Always ON", "Auto Snap", "Press SET", "HalfS DblClick"),
                .help  = "When should the exposure be adjusted for ETTR:",
                .help2 = "Always ON: when you take a pic, or continuously in LiveView\n"
                         "Auto Snap: after u take a pic,trigger another pic if needed\n"
                         "Press SET: meter for ETTR when you press SET (LiveView)\n"
                         "HalfS DblClick: meter for ETTR when pressing halfshutter 2x\n"
            },
            {
                .name = "Exposure target",
                .priv = &auto_ettr_target_level,
                .min = -4,
                .max = 0,
                .choices = CHOICES("-4 EV", "-3 EV", "-2 EV", "-1 EV", "-0.5 EV"),
                .help = "Exposure target for ETTR. Recommended: -0.5 or -1 EV.",
            },
            {
                .name = "Highlight ignore",
                .priv = &auto_ettr_ignore,
                .min = 0,
                .max = 500,
                .unit = UNIT_PERCENT_x10,
                .icon_type = IT_PERCENT,
                .help  = "How many bright pixels are allowed above the target level.",
                .help2 = "Use this to allow spec(ta)cular highlights to be clipped.",
            },
            {
                .name = "Clipping mode",
                .priv = &auto_ettr_clip,
                .max = 2,
                .choices = CHOICES("No clipping", "Clip GREEN", "Clip ANY"),
                .help = "Choose what color channels are allowed to be clipped.",
            },
            {
                .name = "Slowest shutter",
                .priv = &auto_ettr_max_shutter,
                .select = auto_ettr_max_shutter_toggle,
                .update = auto_ettr_max_shutter_update,
                .min = 16,
                .max = 152,
                .icon_type = IT_PERCENT,
                .help = "Slowest shutter speed for ETTR."
            },
            {
                .name = "Link to Canon shutter",
                .priv = &auto_ettr_adjust_mode,
                .max = 1,
                .help = "Hack to adjust slowest shutter from main dial.",
            },
            MENU_EOL,
        },
    },
};

static unsigned int ettr_init()
{
    menu_add("Expo", ettr_menu, COUNT(ettr_menu));
    return 0;
}

static unsigned int ettr_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(ettr_init)
    MODULE_DEINIT(ettr_deinit)
MODULE_INFO_END()

MODULE_STRINGS_START()
    MODULE_STRING("Description", "Auto ETTR")
    MODULE_STRING("License", "GPL")
    MODULE_STRING("Author", "a1ex")
MODULE_STRINGS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_VSYNC_SETPARAM, auto_ettr_vsync_cbr, 0)
    MODULE_CBR(CBR_KEYPRESS, auto_ettr_keypress_cbr, 0)
    MODULE_CBR(CBR_SHOOT_TASK, auto_ettr_polling_cbr, 0)
MODULE_CBRS_END()

MODULE_PROPHANDLERS_START()
    MODULE_PROPHANDLER(PROP_GUI_STATE)
MODULE_PROPHANDLERS_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(auto_ettr)
    MODULE_CONFIG(auto_ettr_trigger)
    MODULE_CONFIG(auto_ettr_ignore)
    MODULE_CONFIG(auto_ettr_target_level)
    MODULE_CONFIG(auto_ettr_max_shutter)
    MODULE_CONFIG(auto_ettr_clip)
    MODULE_CONFIG(auto_ettr_adjust_mode)
MODULE_CONFIGS_END()
