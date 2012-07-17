/** \file
 * Focus control.
 *
 * Support focus stacking and other focus controls.
 * \todo Figure out how to really tell if a focus event is over.  The
 * property PROP_LV_FOCUS_DONE doesn't seem to really indicate that it
 * is safe to send another one.
 */
#include "dryos.h"
#include "menu.h"
#include "bmp.h"
#include "lens.h"
#include "config.h"
#include "ptp.h"

void trap_focus_toggle_from_af_dlg();
void lens_focus_enqueue_step(int dir);

int override_zoom_buttons; // while focus menu is active and rack focus items are selected

int should_override_zoom_buttons()
{
    return (override_zoom_buttons && !is_manual_focus() && lv);// && get_menu_advanced_mode());
}

CONFIG_INT( "focus.stepsize", lens_focus_stepsize, 2 );
CONFIG_INT( "focus.delay", lens_focus_delay, 1 );
CONFIG_INT( "focus.wait", lens_focus_waitflag, 1 );
CONFIG_INT( "focus.rack.delay", focus_rack_delay, 2);


// all focus commands from this module are done with the configured step size and delay
int LensFocus(int num_steps)
{
    return lens_focus(num_steps, lens_focus_stepsize, lens_focus_waitflag, (1<<lens_focus_delay) * 10);
}

int LensFocus2(int num_steps, int step_size)
{
    return lens_focus(num_steps, step_size, lens_focus_waitflag, (1<<lens_focus_delay) * 10);
}


CONFIG_INT( "focus.stack", focus_stack_enabled, 0);
CONFIG_INT( "focus.step",   focus_stack_steps_per_picture, 5 );
//~ CONFIG_INT( "focus.count",  focus_stack_count, 5 );
#define FOCUS_STACK_COUNT (ABS(focus_task_delta) / focus_stack_steps_per_picture + 1)

CONFIG_INT( "focus.follow", follow_focus, 0 );
CONFIG_INT( "focus.follow.mode", follow_focus_mode, 0 ); // 0=arrows, 1=LCD sensor
CONFIG_INT( "focus.follow.rev.h", follow_focus_reverse_h, 0); // for left/right buttons
CONFIG_INT( "focus.follow.rev.v", follow_focus_reverse_v, 0); // for up/down buttons

static int focus_dir;
int get_focus_dir() { return focus_dir; }
int is_follow_focus_active() 
{ 
    if (!follow_focus) return 0;
    if (!lv) return 0;
    if (is_manual_focus()) return 0;
    if (!liveview_display_idle()) return 0;
    if (gui_menu_shown()) return 0;
    if (display_sensor && get_lcd_sensor_shortcuts() && follow_focus_mode==0) return 0;
    if (get_halfshutter_pressed()) return 0;
    return 1;
}

int get_follow_focus_mode()
{
    #if defined(CONFIG_550D) || defined(CONFIG_500D) || defined(CONFIG_5D2)
    return follow_focus_mode;
    #else
    return 0; // no LCD sensor, use arrows only
    #endif
}


int get_follow_focus_dir_v() { return follow_focus_reverse_v ? -1 : 1; }
int get_follow_focus_dir_h() { return follow_focus_reverse_h ? -1 : 1; }

#define FOCUS_MAX 1700
//~ static int focus_position;

//~ static struct semaphore * focus_stack_sem;




/*static void
focus_stack_unlock( void * priv )
{
    gui_stop_menu();
    give_semaphore( focus_stack_sem );
}*/


static void
display_lens_hyperfocal(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    unsigned        menu_font = selected ? MENU_FONT_SEL : MENU_FONT;
    unsigned        font = FONT(FONT_MED, FONT_FG(menu_font), FONT_BG(menu_font));
    unsigned        height = fontspec_height( font );

    menu_draw_icon(x, y + height * 2 + 3, MNI_BOOL(lens_info.name[0] && lens_info.focus_dist && lens_info.raw_aperture), 0);

    y += 10;
    y += height;

    if (!lens_info.name[0])
    {
        y += height;
        bmp_printf( font, x, y,
            "Lens: manual (without chip)."
        );
        return;
    }

    bmp_printf( font, x, y,
        "Lens: %s, %dmm f/%d.%d",
        lens_info.name,
        lens_info.focal_len, 
        lens_info.aperture / 10,
        lens_info.aperture % 10
    );
    
    if (!lv || !lens_info.focus_dist)
    {
        y += height;
        bmp_printf( font, x, y,
            "Hyperfocal: %s",
            lens_format_dist( lens_info.hyperfocal )
        );
        y += height;
        bmp_printf( font, x, y,
            "Your lens did not report focus distance"
        );
        return;
    }

    y += height;

    bmp_printf( font, x, y,
        "Focus dist: %s",
        lens_info.focus_dist == 0xFFFF
                        ? " Infty"
                        : lens_format_dist( lens_info.focus_dist * 10 )
    );

    y += height;
    bmp_printf( font, x, y,
        "Hyperfocal: %s",
        lens_format_dist( lens_info.hyperfocal )
    );

    x += 300;
    y -= height;
    bmp_printf( font, x, y,
        "DOF Near:   %s",
        lens_format_dist( lens_info.dof_near )
    );

    y += height;
    bmp_printf( font, x, y,
        "DOF Far:    %s",
        lens_info.dof_far >= 1000*1000
            ? " Infty"
            : lens_format_dist( lens_info.dof_far )
    );
}


void focus_stack_ensure_preconditions()
{
    while (lens_info.job_state) msleep(100);
    if (!lv)
    {
        while (!lv)
        {
            get_out_of_play_mode(500);
            if (!lv) force_liveview();
            if (lv) break;
            NotifyBoxHide();
            NotifyBox(2000, "Please switch to LiveView");
            msleep(200);
        }
        msleep(200);
    }

    if (is_movie_mode())
    {
        #ifdef CONFIG_5D2
            set_expsim(1);
        #else
            while (is_movie_mode())
            {
                NotifyBox(2000, "Please switch to photo mode");
                msleep(2000);
            }
        #endif
    }

    while (is_manual_focus())
    {
        NotifyBoxHide();
        NotifyBox(2000, "Please enable autofocus");
        msleep(200);
    }

    if (drive_mode != DRIVE_SINGLE) lens_set_drivemode(DRIVE_SINGLE);
    
    msleep(300);
}

// will be called from shoot_task
void
focus_stack(
    int count,
    int num_steps,
    int skip_first
)
{
    NotifyBox(1000, "Focus stack: %dx%d", count, ABS(num_steps) );
    msleep(1000);
    
    int focus_moved_total = 0;

    int i;
    for( i=0 ; i < count ; i++ )
    {
        if (gui_menu_shown()) break;
        
        NotifyBox(1000, "Focus stack: %d of %d", i+1, count );
        
        focus_stack_ensure_preconditions();
        
        if (i > 0 || !skip_first)
        {
            assign_af_button_to_star_button();
            hdr_shot(0,1);
            msleep(300);
            restore_af_button_assignment();
        }

        if( count-1 == i )
            break;
        
        focus_stack_ensure_preconditions();
        
        //~ int num_steps = ((total_steps * (i+1) / (count-1))) - (total_steps * i / (count-1));
        
        NotifyBox(1000, "Focusing..."); msleep(500);
        if (LensFocus(num_steps) == 0)
            break;
        focus_moved_total += num_steps;
    }

    msleep(1000);
    NotifyBoxHide();

    if (i >= count-1)
    {
        hdr_create_script(count, skip_first, 1, file_number - count + 1);
        NotifyBox(2000, "Focus stack done!" );
    }
    else
        NotifyBox(2000, "Focus stack error :(" );

    // Restore to the starting focus position
    focus_stack_ensure_preconditions();
    
    LensFocus(-focus_moved_total);
}

/*
static void
focus_stack_task( void* unused )
{
    while(1)
    {
        take_semaphore( focus_stack_sem, 0 );
        msleep( 500 );
        focus_stack( FOCUS_STACK_COUNT, focus_stack_steps_per_picture );
    }
}*/

//~ TASK_CREATE( "fstack_task", focus_stack_task, 0, 0x1c, 0x1000 );

static struct semaphore * focus_task_sem;
static int focus_task_dir_n_speedx;
static int focus_task_delta;
static int focus_rack_delta;

int is_focus_stack_enabled() { return focus_stack_enabled && focus_task_delta; }

void focus_stack_run(int skip_first)
{
    //~ focus_stack( focus_stack_count, -focus_task_delta, skip_first );
    focus_stack( FOCUS_STACK_COUNT, SGN(-focus_task_delta) * focus_stack_steps_per_picture, skip_first );
}

void focus_stack_trigger_from_menu_work()
{
    if (focus_task_delta == 0) { beep(); return; }
    msleep(1000);
    gui_stop_menu();
    NotifyBox(2000, "Focus stack..."); msleep(2000);
    focus_stack_enabled = 1;
    schedule_remote_shot();
    msleep(1000);
    focus_stack_enabled = 0;
}

void focus_stack_trigger_from_menu(void* priv, int delta)
{
    run_in_separate_task(focus_stack_trigger_from_menu_work, 0);
}

int is_rack_focus_enabled() { return focus_task_delta ? 1 : 0; }

void follow_focus_reverse_dir()
{
    focus_task_dir_n_speedx = -focus_task_dir_n_speedx;
}

void plot_focus_status()
{
    if (gui_menu_shown()) return;
    bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 160, "%s        ", focus_task_dir_n_speedx > 0 ? "FAR " : focus_task_dir_n_speedx < 0 ? "NEAR" : "    ");
}

static void
focus_dir_display( 
    void *          priv,
    int         x,
    int         y,
    int         selected
) {

    bmp_printf(
        selected ? MENU_FONT_SEL : MENU_FONT,
        x, y,
        "Focus dir     : %s",
        focus_dir ? "FAR " : "NEAR"
    );
    menu_draw_icon(x, y, MNI_ON, 0);
}

static void
focus_show_a( 
    void *          priv,
    int         x,
    int         y,
    int         selected
) {
    if (selected) override_zoom_buttons = 1;
    bmp_printf(
        !selected ? MENU_FONT : should_override_zoom_buttons() ? FONT(FONT_LARGE,COLOR_WHITE,0x12) : MENU_FONT_SEL,
        x, y,
        "Focus End Point:%s%d%s",
        focus_task_delta > 0 ? "+" : 
        focus_task_delta < 0 ? "-" : " ",
        ABS(focus_task_delta),
        focus_task_delta ? "steps from here" : ", not configured."
    );
    menu_draw_icon(x, y, MNI_BOOL(focus_task_delta), 0);
}

/*static void
focus_show_b( 
    void *          priv,
    int         x,
    int         y,
    int         selected
) {
    if (selected) override_zoom_buttons = 1;
    bmp_printf(
        !selected ? MENU_FONT : should_override_zoom_buttons() ? FONT(FONT_LARGE,COLOR_WHITE,0x12) : MENU_FONT_SEL,
        x, y,
        "Focus Start Pt.: Here. Go%s%dsteps.",
        focus_task_delta > 0 ? "+" :      //|
        focus_task_delta < 0 ? "-" : " ",
        ABS(focus_task_delta)
    );
    menu_draw_icon(x, y, MNI_BOOL(focus_task_delta), 0);
}*/

static void
rack_focus_print( 
    void *          priv,
    int         x,
    int         y,
    int         selected
) {
    extern int lcd_release_running;
    bmp_printf(
        MENU_FONT,
        x, y,
        "Rack focus %s",
        lcd_release_running && lcd_release_running < 3 && recording ? "(also w. LCD sensor)" : ""
    );
    menu_draw_icon(x, y, MNI_ACTION, 0);
}


static void
focus_reset_a( void * priv, int delta )
{
    focus_task_delta = 0;
    menu_show_only_selected();
}

static void
focus_alter_a( void * priv, int delta )
{
    menu_show_only_selected();
    lens_focus_enqueue_step(delta);
}

int focus_rack_auto_record = 0;
int focus_rack_enable_delay = 1;

static void
focus_toggle( void * priv )
{
    if (focus_rack_delta) return; // another rack focus operation in progress
    menu_show_only_selected();
    focus_task_delta = -focus_task_delta;
    focus_rack_delta = focus_task_delta;
    give_semaphore( focus_task_sem );
}

// called from shortcut keys (MENU/PLAY) and from LCD remote shot
void
rack_focus_start_now( void * priv, int delta )
{
    focus_rack_auto_record = 0;
    focus_rack_enable_delay = 0;
    focus_toggle(priv);
}

static void
rack_focus_start_delayed( void * priv, int delta )
{
    focus_rack_auto_record = 0;
    focus_rack_enable_delay = 1;
    focus_toggle(priv);
}

static void
rack_focus_start_auto_record( void * priv, int delta )
{
    focus_rack_auto_record = 1;
    focus_rack_enable_delay = 1;
    focus_toggle(priv);
}

static void
focus_stepsize_toggle(void* priv, int delta)
{
    lens_focus_stepsize = mod(lens_focus_stepsize - 1 + delta, 3) + 1;
}

static void
focus_rack_speed_display(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    bmp_printf(
        MENU_FONT,
        x, y,
        "Focus StepSize : %d",
        lens_focus_stepsize
    );
    menu_draw_icon(x, y, MNI_PERCENT, lens_focus_stepsize * 100 / 3);
}

static void
focus_delay_display(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    bmp_printf(
        MENU_FONT,
        x, y,
        "Focus StepDelay: %s%dms",
        lens_focus_waitflag ? "Wait + " : "",
        (1 << lens_focus_delay) * 10
    );
    menu_draw_icon(x, y, MNI_PERCENT, lens_focus_delay * 100 / 9);
}

static void
focus_delay_sub_display(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    bmp_printf(
        MENU_FONT,
        x, y,
        "Focus delay   : %dms",
        (1 << lens_focus_delay) * 10
    );
    menu_draw_icon(x, y, MNI_PERCENT, lens_focus_delay * 100 / 9);
}


/*
static void
focus_stack_count_increment( void * priv )
{
    focus_stack_count = focus_stack_count * 2;
    if (focus_stack_count == 256) focus_stack_count = 5;
    if (focus_stack_count > 200) focus_stack_count = 2;
}*/

static void
focus_delay_toggle( void* priv, int sign)
{
    lens_focus_delay = mod(lens_focus_delay + sign, 7);
}

static void
focus_stack_print(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    bmp_printf(
        MENU_FONT,
        x, y,
        "Stack focus    : %s,stepsize=%d",
        focus_stack_enabled ? "SNAP" : "PLAY",
        focus_stack_steps_per_picture
    );
    bmp_printf(
        FONT_MED,
        x + font_large.width * 17, y + font_large.height,
        "(will take %d pictures)",
        focus_task_delta ? FOCUS_STACK_COUNT : 0
    );
    if (!focus_task_delta)
        menu_draw_icon(x, y, MNI_OFF, 0);
    if (!focus_stack_enabled)
        menu_draw_icon(x, y, MNI_ACTION, 0);
}

void
lens_focus_start(
    int     dir
)
{
    if( dir == 0 )
        focus_task_dir_n_speedx = focus_dir ? 1 : -1;
    else
        focus_task_dir_n_speedx = dir; // this includes the focus speed multiplier (1 = FF+, 2 = FF++)

    give_semaphore( focus_task_sem );
}

int queued_focus_steps = 0;
void lens_focus_enqueue_step(int dir)
{
    queued_focus_steps += ABS(dir);
    lens_focus_start(SGN(dir));
}


void
lens_focus_stop( void )
{
    focus_task_dir_n_speedx = 0;
}

static void
rack_focus(
    int     speed,
    int     delta
)
{
    DebugMsg( DM_MAGIC, 3,
        "%s: speed=%d delta=%d",
        __func__,
        speed,
        delta
    );

    if( speed <= 0 )
        speed = 1;

    int     speed_cmd = speed;

    // If we are moving closer, invert the speed command
    if( delta < 0 )
    {
        speed_cmd = -speed;
        delta = -delta;
    }

    speed_cmd = speed_cmd > 0 ? 1 : -1;

    while( delta )
    {
        delta --;

        //~ bmp_printf(FONT_LARGE, os.x0 + 50, os.y0 + 50, "Rack Focus: %d%% ", ABS(delta0 - delta) * 100 / ABS(delta0));
        
        if (LensFocus( speed_cmd ) == 0) break;
        //~ gui_hide_menu( 10 );
    }
}

static void rack_focus_wait()
{
    if (focus_rack_delay && focus_rack_enable_delay)
    {
        wait_till_next_second();
        for (unsigned int i = 0; i < focus_rack_delay; i++)
        {
            NotifyBox(2000, "Rack Focus: %d...", focus_rack_delay - i);
            wait_till_next_second();
        }
    }
}

static void
focus_task( void* unused )
{
    TASK_LOOP
    {
        msleep(50);
        int err = take_semaphore( focus_task_sem, 500 );
        if (err) continue;
        
        while (lens_info.job_state) msleep(100);

        if( focus_rack_delta )
        {

            //~ gui_hide_menu(50);
            int movie_started_by_ml = 0;
            if (focus_rack_auto_record)
            {
                menu_stop();
                NotifyBox(2000, "Rack Focus: REC Start");
                ensure_movie_mode();
                if (!recording)
                {
                    movie_started_by_ml = 1;
                    movie_start();
                }
            }
            
            rack_focus_wait();
            //~ gui_hide_menu( 10 );
            rack_focus(
                lens_focus_stepsize,
                focus_rack_delta
            );

            focus_rack_delta = 0;

            if (movie_started_by_ml)
            {
                rack_focus_wait();
                NotifyBox(2000, "Rack Focus: REC Stop");
                movie_end();
            }

            continue;
        }

        while( focus_task_dir_n_speedx )
        {
            int f = focus_task_dir_n_speedx; // avoids race condition, as focus_task_dir_n_speedx may be changed from other tasks
            if (LensFocus2(1, f * lens_focus_stepsize) == 0) 
            {
                queued_focus_steps = 0;
                focus_task_dir_n_speedx = 0;
                break;
            }
            focus_task_delta += f;
            menu_set_dirty();
            
            if (queued_focus_steps)
            {
                queued_focus_steps--;
                if (queued_focus_steps == 0) focus_task_dir_n_speedx = 0;
            }
        }
    }
}

TASK_CREATE( "focus_task", focus_task, 0, 0x18, 0x1000 );


//~ PROP_HANDLER( PROP_LV_FOCUS )
//~ {
    //~ return prop_cleanup( token, property );
    //~ static int16_t oldstep = 0;
    //~ const struct prop_focus * const focus = (void*) buf;
    //~ const int16_t step = (focus->step_hi << 8) | focus->step_lo;
    //~ bmp_printf( FONT_SMALL, 200, 30,
        //~ "FOCUS: %08x active=%02x dir=%+5d (%04x) mode=%02x",
            //~ *(unsigned*)buf,
            //~ focus->active,
            //~ (int) step,
            //~ (unsigned) step & 0xFFFF,
            //~ focus->mode
        //~ );
    //~ return prop_cleanup( token, property );
//~ }

static void
follow_focus_toggle_dir_h( void * priv )
{
    follow_focus_reverse_h = !follow_focus_reverse_h;
}
static void
follow_focus_toggle_dir_v( void * priv )
{
    follow_focus_reverse_v = !follow_focus_reverse_v;
}

static void
follow_focus_print(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    bmp_printf(
        selected ? MENU_FONT_SEL : MENU_FONT,
        x, y,
        "Follow Focus   : %s",
        follow_focus == 0 ? "OFF" :
        get_follow_focus_mode() == 0 ? "Arrows" : "LCD sensor"
    );
    if (follow_focus == 1)
    {
        bmp_printf(FONT_MED, x + 580, y+5, follow_focus_reverse_h ? "- +" : "+ -");
        bmp_printf(FONT_MED, x + 580 + font_med.width, y-4, follow_focus_reverse_v ? "-\n+" : "+\n-");
    }
    if (follow_focus)
    {
        if (is_manual_focus()) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "Follow focus requires autofocus enabled.");
        if (!lv) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "Follow focus only works in LiveView.");
    }
    menu_draw_icon(x, y, MNI_BOOL_LV(follow_focus));
}

CONFIG_INT("movie.af", movie_af, 0);
#ifdef CONFIG_MOVIE_AF
CONFIG_INT("movie.af.aggressiveness", movie_af_aggressiveness, 4);
CONFIG_INT("movie.af.noisefilter", movie_af_noisefilter, 7); // 0 ... 9
int movie_af_stepsize = 10;
#endif


int focus_value = 0; // heuristic from 0 to 100
int focus_value_delta = 0;
int focus_min_value = 0; // to confirm focus variation 

volatile int focus_done = 0;
volatile uint32_t focus_done_raw = 0;

PROP_HANDLER(PROP_LV_FOCUS_DONE)
{
        focus_done_raw = buf[0];
        focus_done = 1;
}


int get_focus_graph() 
{ 
    if (should_draw_bottom_graphs())
        return zebra_should_run();

    if (movie_af && is_movie_mode())
        return !is_manual_focus() && zebra_should_run();

    if (get_trap_focus() && can_lv_trap_focus_be_active())
        return (liveview_display_idle() && get_global_draw()) || get_halfshutter_pressed();

    return 0;
}

int lv_focus_confirmation = 0;
static int hsp_countdown = 0;
int can_lv_trap_focus_be_active()
{
    #ifdef CONFIG_50D
    return 0;
    #endif
    //~ bmp_printf(FONT_MED, 100, 100, "LVTF 0 lv=%d hsc=%d dof=%d sm=%d gs=%d sp=%d mf=%d",lv,hsp_countdown,dofpreview,shooting_mode,gui_state,get_silent_pic_mode(),is_manual_focus());
    if (!lv) return 0;
    if (hsp_countdown) return 0; // half-shutter can be mistaken for DOF preview, but DOF preview property triggers a bit later
    if (dofpreview) return 0;
    if (is_movie_mode()) return 0;
    if (gui_state != GUISTATE_IDLE) return 0;
    if (get_silent_pic()) return 0;
    if (!is_manual_focus()) return 0;
    //~ bmp_printf(FONT_MED, 100, 100, "LVTF 1");
    return 1;
}

static int hsp = 0;
int movie_af_reverse_dir_request = 0;
PROP_HANDLER(PROP_HALF_SHUTTER)
{
    if (buf[0] && !hsp) movie_af_reverse_dir_request = 1;
    hsp = buf[0];
    hsp_countdown = 3;
    if (get_zoom_overlay_trigger_mode() <= 2) zoom_overlay_set_countdown(0);
}


int get_lv_focus_confirmation() 
{ 
    if (!can_lv_trap_focus_be_active()) return 0;
    if (!get_halfshutter_pressed()) return 0;
    int ans = lv_focus_confirmation;
    lv_focus_confirmation = 0;
    return ans; 
}

int is_manual_focus()
{
    return (af_mode & 0xF) == 3;
}

#ifdef CONFIG_MOVIE_AF
int movie_af_active()
{
    return is_movie_mode() && lv && !is_manual_focus() && (focus_done || movie_af==3);
}

static void movie_af_step(int mag)
{
    if (!movie_af_active()) return;
    
    #define MAXSTEPSIZE 64
    #define NP ((int)movie_af_noisefilter)
    #define NQ (10 - NP)
    
    int dirchange = 0;
    static int dir = 1;
    static int prev_mag = 0;
    static int target_focus_rate = 1;
    if (mag == prev_mag) return;
    
    bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 180, "    ");

    static int dmag = 0;
    dmag = ((mag - prev_mag) * NQ + dmag * NP) / 10; // focus derivative is filtered (it's noisy)
    int dmagp = dmag * 10000 / prev_mag;
    static int dmagp_acc = 0;
    static int acc_num = 0;
    dmagp_acc += dmagp;
    acc_num++;
    
    if (focus_done_raw & 0x1000) // bam! focus motor has hit something
    {
        dirchange = 1;
        bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 180, "BAM!");
    }
    else if (movie_af_reverse_dir_request)
    {
        dirchange = 1;
        movie_af_reverse_dir_request = 0;
        bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 180, "REV ");
    }
    else
    {
        if (dmagp_acc < -500 && acc_num >= 2) dirchange = 1;
        if (ABS(dmagp_acc) < 500)
        {
            bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 180, " !! "); // confused
        }
        else
        {
            dmagp_acc = 0;
            acc_num = 0;
            bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 180, " :) "); // it knows exactly if it's going well or not
        }

        if (ABS(dmagp) > target_focus_rate) movie_af_stepsize /= 2;       // adjust step size in order to maintain a preset rate of change in focus amount
        else movie_af_stepsize = movie_af_stepsize * 3 / 2;               // when focus is "locked", derivative of focus amount is very high => step size will be very low
        movie_af_stepsize = COERCE(movie_af_stepsize, 2, MAXSTEPSIZE);    // when OOF, derivative is very small => will increase focus speed
    }
    
    if (dirchange)
    {
        dir = -dir;
        dmag = 0;
        target_focus_rate /= 4;
    }
    else
    {
        target_focus_rate = target_focus_rate * 11/10;
    }
    target_focus_rate = COERCE(target_focus_rate, movie_af_aggressiveness * 20, movie_af_aggressiveness * 100);

    focus_done = 0; 
    static int focus_pos = 0;
    int focus_delta = movie_af_stepsize * SGN(dir);
    focus_pos += focus_delta;
    LensFocus(focus_delta);  // send focus command

    //~ bmp_draw_rect(7, COERCE(350 + focus_pos, 100, 620), COERCE(380 - mag/200, 100, 380), 2, 2);
    
    if (get_global_draw())
    {
        bmp_fill(0, 8, 151, 128, 10);                                          // display focus info
        bmp_fill(COLOR_RED, 8, 151, movie_af_stepsize, 5);
        bmp_fill(COLOR_BLUE, 8, 156, 64 * target_focus_rate / movie_af_aggressiveness / 100, 5);
        bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 160, "%s %d%%   ", dir > 0 ? "FAR " : "NEAR", dmagp/100);
    }
    prev_mag = mag;
}
#endif

#define NMAGS 64
static int mags[NMAGS] = {0};
#define FH COERCE(mags[i] * 45 / maxmagf, 0, 54)
int maxmagf = 1;
int trap_focus_autoscaling = 1;
int minmag = 0;
int focus_graph_dirty = 0;

static void update_focus_mag(int mag)
{
    focus_graph_dirty = 1;
    int maxmag = 1;
    int minmag = 100000000;
    int i;
    #if defined(CONFIG_550D) || defined(CONFIG_500D)
    #define WEIGHT(i) (i > 40 ? 1 : 0.2)
    #else
    #define WEIGHT(i) 1
    #endif
    for (i = 0; i < NMAGS-1; i++)
    {
        if (mags[i] * WEIGHT(i) > maxmag) maxmag = mags[i] * WEIGHT(i);
        if (mags[i] < minmag) minmag = mags[i];
    }
    
    if (trap_focus_autoscaling)
        maxmagf = (maxmagf * 9 + maxmag * 1) / 10;
    
    for (i = 0; i < NMAGS-1; i++)
        mags[i] = mags[i+1];
    mags[i] = mag;

    focus_value_delta = FH * 2 - focus_value;
    focus_value = (focus_value + FH * 2) / 2;
    focus_min_value = COERCE(minmag * 45 / maxmagf, 0, 50) * 2;
    lv_focus_confirmation = (focus_value > 80 && focus_min_value < 60);
}
void plot_focus_mag()
{

    int x0 = 8;
    int y0 = 100;
    if (should_draw_bottom_graphs())
    {
        x0 = 500;
        y0 = 480-54;
    }
    
    if (gui_state != GUISTATE_IDLE) return;
    if (!lv) return;
    if (!get_global_draw()) return;
    
    BMP_LOCK(
        int i;
        for (i = 0; i < NMAGS-1; i++)
        {
            bmp_draw_rect(COLOR_BLACK, x0 + 2*i, y0, 1, 54);
            bmp_draw_rect(trap_focus_autoscaling ? 70 : COLOR_RED, x0 + 2*i, y0 + 54 - FH, 1, FH);
        }
        //~ ff_check_autolock();
        bmp_draw_rect(50, x0-1, y0-1, NMAGS*2, 54);
    )
}
#undef FH
#undef NMAGS

int handle_trap_focus(struct event * event)
{
    if (event->param == BGMT_PRESS_SET && get_trap_focus() && can_lv_trap_focus_be_active() && zebra_should_run())
    {
        trap_focus_autoscaling = !trap_focus_autoscaling;
        return 0;
    }
    return 1;
}

/*
void ff_check_autolock()
{
    static int rev_countdown = 0;
    static int stop_countdown = 0;
    if (is_follow_focus_active())
    {
        if (get_global_draw())
        {
            plot_focus_status();
            bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 180, "    ");
        }
        if (get_follow_focus_stop_on_focus() && !stop_countdown)
        {
            if (!rev_countdown)
            {
                if (focus_value - focus_value_delta*5 > 110)
                {
                    follow_focus_reverse_dir();
                    rev_countdown = 5;
                    bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 180, "PEAK");
                }
            }
            else
            {
                rev_countdown--;
                bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 180, "PEAK");
                if (focus_value + focus_value_delta*5 > 110) rev_countdown = 0;
                if (!rev_countdown)
                {
                    bmp_printf(FONT(FONT_MED, COLOR_WHITE, 0), 30, 180, "LOCK");
                    lens_focus_stop();
                    stop_countdown = 10;
                }
            }
        }
        if (stop_countdown) stop_countdown--;
    }
}*/

int focus_mag_a = 0;
int focus_mag_b = 0;
int focus_mag_c = 0;
PROP_HANDLER(PROP_LV_FOCUS_DATA)
{
    focus_done = 1;
    focus_mag_a = buf[2];
    focus_mag_b = buf[3];
    focus_mag_c = buf[4];
    #if defined(CONFIG_600D) || defined(CONFIG_1100D)
    int focus_mag = focus_mag_c;
    #else
    int focus_mag = focus_mag_a + focus_mag_b;
    #endif
    
#ifdef CONFIG_MOVIE_AF
    if (movie_af != 3)
#endif
    {
        update_focus_mag(focus_mag);
#ifdef CONFIG_MOVIE_AF
        if ((movie_af == 2) || (movie_af == 1 && get_halfshutter_pressed())) 
            movie_af_step(focus_mag);
#endif
    }
}

#ifdef CONFIG_MOVIE_AF
static void
movie_af_print(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    if (movie_af)
        bmp_printf(
            selected ? MENU_FONT_SEL : MENU_FONT,
            x, y,
            "Movie AF      : %s A%d N%d",
            movie_af == 1 ? "Hold" : movie_af == 2 ? "Cont" : movie_af == 3 ? "CFPk" : "Err",
            movie_af_aggressiveness,
            movie_af_noisefilter
        );
    else
        bmp_printf(
            selected ? MENU_FONT_SEL : MENU_FONT,
            x, y,
            "Movie AF      : OFF"
        );
}

void movie_af_aggressiveness_bump(void* priv)
{
    movie_af_aggressiveness = movie_af_aggressiveness * 2;
    if (movie_af_aggressiveness > 64) movie_af_aggressiveness = 1;
}
void movie_af_noisefilter_bump(void* priv)
{
    movie_af_noisefilter = (movie_af_noisefilter + 1) % 10;
}
#endif

static void
focus_misc_task(void* unused)
{
    TASK_LOOP
    {
        msleep(100);
        
        if (hsp_countdown) hsp_countdown--;

#ifdef CONFIG_MOVIE_AF
        if (movie_af == 3)
        {
            int fm = get_spot_focus(100);
            update_focus_mag(fm);
            //~ if (get_focus_graph()) plot_focus_mag();
            movie_af_step(fm);
        }
#endif

        if (focus_graph_dirty && get_focus_graph()) 
        {
            plot_focus_mag();
            focus_graph_dirty = 0;
        }
        
#ifdef CONFIG_60D
        if (CURRENT_DIALOG_MAYBE_2 == DLG2_FOCUS_MODE && is_manual_focus())
#else
        if (CURRENT_DIALOG_MAYBE == DLG_FOCUS_MODE && is_manual_focus())
#endif
        {   
            #ifndef CONFIG_50D
            trap_focus_toggle_from_af_dlg();
            #endif
            
            #ifdef CONFIG_60D
            while (CURRENT_DIALOG_MAYBE_2 == DLG2_FOCUS_MODE) msleep(100);
            #else
            while (CURRENT_DIALOG_MAYBE == DLG_FOCUS_MODE) msleep(100);
            #endif
        }
    }
}

TASK_CREATE( "focus_misc_task", focus_misc_task, 0, 0x1e, 0x1000 );

static void 
trap_focus_display( void * priv, int x, int y, int selected )
{
    int t = (*(int*)priv);
    bmp_printf(
        selected ? MENU_FONT_SEL : MENU_FONT,
        x, y,
        "Trap Focus     : %s",
        t == 1 ? "Hold AF button" : t == 2 ? "Continuous" : "OFF"
    );
    if (t)
    {
        if (is_movie_mode()) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "Trap focus only works in photo mode.");
        if (!is_manual_focus()) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "Trap focus only works with manual focus.");
        if (!lv && !lens_info.name[0]) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "Trap focus outside LiveView requires a chipped lens");
        #ifdef CONFIG_50D
        if (lv) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "On 50D, trap focus doesn't work in LiveView.");
        #endif
        if (lv && is_movie_mode()) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "Trap focus does not work in movie mode.");
        if (t == 2 && cfn_get_af_button_assignment()) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "Assign AF button to half-shutter from CFn!");
    }
}


extern int trap_focus;

void trap_focus_toggle_from_af_dlg()
{
    trap_focus = !trap_focus;
    clrscr();
    NotifyBoxHide();
    NotifyBox(2000, "Trap Focus: %s", trap_focus ? "ON" : "OFF");
    msleep(10);
    SW1(1,10);
    SW1(0,0);
    if (beep_enabled) beep();
    if (trap_focus) info_led_blink(3, 50, 50);
    else info_led_blink(1, 50, 50);
}

CONFIG_INT("focus.patterns", af_patterns, 0);

static void
afp_display(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    bmp_printf(
        selected ? MENU_FONT_SEL : MENU_FONT,
        x, y,
        "Focus Patterns : %s",
        af_patterns ? "ON" : "OFF"
    );
    if (af_patterns)
    {
        if (lv) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "Focus patterns won't work in LiveView");
        if (!lens_info.name[0]) menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "Focus patterns require a chipped lens");
    }
}

static struct menu_entry focus_menu[] = {
    {
        .name = "Trap Focus",
        .priv       = &trap_focus,
        .max = 2,
        .display    = trap_focus_display,
        .help = "Takes a picture when the subject comes in focus. MF only.",
        //.essential = FOR_PHOTO,
        .icon_type = IT_BOOL,
    },
#ifndef CONFIG_5DC
    {
        .name = "Focus Patterns",
        .priv = &af_patterns,
        .display    = afp_display,
        .select = menu_binary_toggle,
        .help = "Custom AF patterns (photo mode only; ported from 400plus)",
        //.essential = FOR_PHOTO_NON_LIVEVIEW,
    },
    {
        .name = "Follow Focus",
        .priv = &follow_focus,
        .display    = follow_focus_print,
        .select     = menu_binary_toggle,

        .help = "Focus with arrow keys. MENU while REC = save focus point.",
        //.essential = FOR_LIVEVIEW,

        .children =  (struct menu_entry[]) {
            #if defined(CONFIG_550D) || defined(CONFIG_500D) || defined(CONFIG_5D2)
            {
                .name = "Focus using",
                .priv = &follow_focus_mode, 
                .max = 1,
                .choices = (const char *[]) {"Arrow keys", "LCD sensor"},
                .help = "You can focus with arrow keys or with the LCD sensor",
            },
            #endif
            {
                .name = "Left/Right dir",
                .priv = &follow_focus_reverse_h, 
                .max = 1,
                .choices = (const char *[]) {"+ / -", "- / +"},
                .help = "Focus direction for Left and Right keys",
            },
            {
                .name = "Up/Down dir",
                .priv = &follow_focus_reverse_v, 
                .max = 1,
                .choices = (const char *[]) {"+ / -", "- / +"},
                .help = "Focus direction for Up and Down keys",
            },
            MENU_EOL
        },

    },
#ifdef CONFIG_MOVIE_AF
    {
        .name = "Movie AF",
        .priv = &movie_af,
        .display    = movie_af_print,
        .select     = menu_quaternary_toggle,
        .select_reverse = movie_af_noisefilter_bump,
        .select_auto = movie_af_aggressiveness_bump,
        .help = "Custom AF algorithm in movie mode. Not very efficient."
    },
#endif
    {
        .name = "Focus StepSize",
        .display    = focus_rack_speed_display,
        .select     = focus_stepsize_toggle,
        .help = "Step size for focus commands (same units as in EOS Utility)",
        //.essential = FOR_LIVEVIEW,
    },
    {
        .name = "Focus StepDelay",
        .priv = &lens_focus_waitflag,
        .display    = focus_delay_display,
        .select     = focus_delay_toggle,
        .help = "Delay between two successive focus commands.",
        //.essential = FOR_LIVEVIEW,
        .children =  (struct menu_entry[]) {
            {
                .name = "Focus delay",
                .display    = focus_delay_sub_display,
                .select     = focus_delay_toggle,
                .help = "Delay between two successive focus commands.",
            },
            {         //"Focus StepDelay"
                .name = "Wait flag",
                .priv = &lens_focus_waitflag,
                .max = 1,
                .help = "Wait for 'focus done' signal before sending next command.",
            },
            MENU_EOL
        },
    },
    /*{
        .name = "Focus Start Point:",
        //~ .display    = focus_show_a,
        .display    = focus_show_b,
        .select     = focus_alter_a,
        .help = "Press ZoomIn, then use Left/Right/Rcrollwheel to set it."
    },*/
    {
        .name = "Focus End Point",
        .display    = focus_show_a,
        .select     = focus_reset_a,
        .help = "Press SET to fix here the end point of rack focus."
    },
    {
        .name = "Rack Delay     ",
        .priv    = &focus_rack_delay,
        .max = 20,
        .icon_type = IT_PERCENT,
        .help = "Number of seconds before starting rack focus."
    },
    {
        .name = "Rack Focus",
        .priv       = "Rack focus",
        .display    = rack_focus_print,
        .select     = rack_focus_start_delayed,
        //~ .select_Q   = rack_focus_start_now,
        .select_reverse = rack_focus_start_auto_record,
        .help = "Press SET for rack focus, or PLAY to also start recording.",
        .icon_type = IT_ACTION,
    },
    /*{
        .name = "Focus dir",
        .priv       = &focus_dir,
        .display    = focus_dir_display,
        .select     = menu_binary_toggle,
        .help = "Focus direction used when you press the 'Zoom In' button."
    },*/
    {
        .name = "Stack focus",
        .priv = &focus_stack_enabled,
        .display    = focus_stack_print,
        .select = menu_binary_toggle,
        .select_reverse     = focus_stack_trigger_from_menu,
        .help = "Focus bracketing, increases DOF while keeping bokeh.",
        .children =  (struct menu_entry[]) {
            {
                .name = "Step Size",
                .priv = &focus_stack_steps_per_picture, 
                .min = 1,
                .max = 10,
                .help = "Focus step size between two pictures.",
            },
            {
                .name = "Trigger mode",
                .priv = &focus_stack_enabled, 
                .max = 1,
                .choices = (const char *[]) {"Press PLAY", "Take a pic"},
                .help = "Choose how to start the focus stacking sequence.",
            },
            MENU_EOL
        },
    },
    {
        .name = "Focus distance",
        .display    = display_lens_hyperfocal,
        .help = "Focus distance and DOF info (read-only)",
        //.essential = FOR_PHOTO | FOR_MOVIE,
    },
#endif
};




static void
focus_init( void* unused )
{
    //~ focus_stack_sem = create_named_semaphore( "focus_stack_sem", 0 );
    focus_task_sem = create_named_semaphore( "focus_task_sem", 1 );

    menu_add( "Focus", focus_menu, COUNT(focus_menu) );
    
}

/*
PTP_HANDLER( 0x9998, 0 )
{
    int step = (int) param1;

    focus_position += step;
    if( focus_position < 0 )
        focus_position = 0;
    else
    if( focus_position > FOCUS_MAX )
        focus_position = FOCUS_MAX;

    lens_focus( 0x7, (int) param1 );
    bmp_printf( FONT_MED, 650, 35, "%04d", focus_position );

    struct ptp_msg msg = {
        .id     = PTP_RC_OK,
        .session    = session,
        .transaction    = transaction,
        .param_count    = 2,
        .param      = { param1, focus_position },
    };

    context->send(
        context->handle,
        &msg
    );

    return 0;
}
*/

INIT_FUNC( __FILE__, focus_init );

int handle_rack_focus(struct event * event)
{
    if (!lv) return 1;
    if (is_manual_focus()) return 1;

    if (recording && !gui_menu_shown())
    {
        if (event->param == BGMT_PLAY) // this should be good as rack focus trigger key too
        {
            rack_focus_start_now(0,0);
            return 0;
        }

        if (event->param == BGMT_MENU && is_follow_focus_active())
        {
            focus_reset_a(0,0);
            NotifyBox(2000, "Focus point saved here.     \n"
                            "To return to it, press PLAY.");
            return 0;
        }
    }
    return 1;
}

int handle_rack_focus_menu_overrides(struct event * event)
{
    if (!lv) return 1;
    if (is_manual_focus()) return 1;
    if (!should_override_zoom_buttons()) return 1;
    
    if (gui_menu_shown() && is_menu_active("Focus"))
    {
        if (menu_active_but_hidden())
        {
            switch(event->param)
            {
                case BGMT_PRESS_LEFT:
                    lens_focus_start(-1 * get_follow_focus_dir_h());
                    return 0;
                case BGMT_PRESS_RIGHT:
                    lens_focus_start(1 * get_follow_focus_dir_h());
                    return 0;
                case BGMT_WHEEL_LEFT:
                    menu_show_only_selected();
                    lens_focus_enqueue_step( -get_follow_focus_dir_h() );
                    return 0;
                case BGMT_WHEEL_RIGHT:
                    menu_show_only_selected();
                    lens_focus_enqueue_step( get_follow_focus_dir_h() );
                    return 0;
            }
        }
        switch(event->param)
        {
            #ifdef BGMT_UNPRESS_UDLR
            case BGMT_UNPRESS_UDLR:
            #else
            case BGMT_UNPRESS_LEFT:
            case BGMT_UNPRESS_RIGHT:
            case BGMT_UNPRESS_UP:
            case BGMT_UNPRESS_DOWN:
            #endif
                lens_focus_stop();
                return 1;
        }
    }
    return 1;
}
int handle_follow_focus(struct event * event)
{
    if (is_follow_focus_active())
    {
        if (get_follow_focus_mode() == 0) // arrows
        {
            switch(event->param)
            {
                case BGMT_PRESS_LEFT:
                    lens_focus_start(-1 * get_follow_focus_dir_h());
                    return 0;
                case BGMT_PRESS_RIGHT:
                    lens_focus_start(1 * get_follow_focus_dir_h());
                    return 0;
                case BGMT_PRESS_UP:
                    lens_focus_start(-2 * get_follow_focus_dir_v());
                    return 0;
                case BGMT_PRESS_DOWN:
                    lens_focus_start(2 * get_follow_focus_dir_v());
                    return 0;
                #ifdef BGMT_UNPRESS_UDLR
                case BGMT_UNPRESS_UDLR:
                #else
                case BGMT_UNPRESS_LEFT:
                case BGMT_UNPRESS_RIGHT:
                case BGMT_UNPRESS_UP:
                case BGMT_UNPRESS_DOWN:
                #endif
                    lens_focus_stop();
                    return 1;
            }
        }
        else lens_focus_stop();
    }
    return 1;
}
