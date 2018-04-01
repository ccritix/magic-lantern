/* 5D3 silent control
 * it has touch sensors on the rear scrollwheel
 * https://www.youtube.com/watch?v=j91MI0hlcSs */

#include "dryos.h"
#include "menu.h"
#include "config.h"
#include "property.h"
#include "bmp.h"
#include "beep.h"

CONFIG_INT("silent.control", silent_control_enabled, 0);

//static PROP_INT(PROP_SILENT_CONTROL_SETTING, silent_control_setting);
static PROP_INT(PROP_Q_POSITION, q_position);
static PROP_INT(PROP_SILENT_CONTROL_STATUS, silent_control_status);

/* fixme: implement reference counting */
void silent_control_request()
{
    if (!silent_control_enabled)
    {
        return;
    }

    int request = 0;
    prop_request_change(PROP_SILENT_CONTROL_STATUS, &request, 4);
}

void silent_control_release()
{
    int request = 2;
    prop_request_change(PROP_SILENT_CONTROL_STATUS, &request, 4);
}

static int silent_pressed = 0;
static int silent_count = 0;
static int action_disabled_time = 0;
static int silent_direction = 0;
static int silent_prev_direction = 0;
static const int long_press_thr = 15;

static inline int action_disabled()
{
    if (action_disabled_time == 0)
    {
        return 0;
    }

    return get_ms_clock() - action_disabled_time < 300;
}

static const int pos_x = 670;
static const int pos_y = 390;

static void triang_h(int x0, int y0, int d, int color)
{
    int dy = ABS(d/2);

    for (int y = y0 - dy; y <= y0 + dy; y++)
    {
        draw_line(x0, y0, x0 + d, y, color);
    }
}

static void triang_v(int x0, int y0, int d, int color)
{
    int dx = ABS(d/2);

    for (int x = x0 - dx; x <= x0 + dx; x++)
    {
        draw_line(x0, y0, x, y0 + d, color);
    }
}

/* called from menu and GuiMainTask; must be thread safe */
void silent_control_draw_indicator()
{
    if (silent_control_status != 1)
    {
        return;
    }

    if (!silent_control_enabled)
    {
        /* fixme */
        return;
    }

    int color_idle = (silent_pressed && !action_disabled()) ? COLOR_GRAY(20) : COLOR_GRAY(5);
    int color_active = (silent_count >= long_press_thr) ? COLOR_RED : COLOR_YELLOW;
    int color_prev_active = color_active;
    int color_inactive = COLOR_BLACK;

    int set_gesture =
        (silent_prev_direction == BGMT_SILENT_RIGHT && silent_direction == BGMT_SILENT_DOWN) ||
        (silent_prev_direction == BGMT_SILENT_DOWN && silent_direction == BGMT_SILENT_RIGHT);

    int qm_gesture =
        (silent_prev_direction == BGMT_SILENT_LEFT && silent_direction == BGMT_SILENT_UP) ||
        (silent_prev_direction == BGMT_SILENT_UP && silent_direction == BGMT_SILENT_LEFT);

    draw_circle(pos_x, pos_y, 27, COLOR_BLACK);
    draw_circle(pos_x, pos_y, 26, COLOR_BLACK);
    fill_circle(pos_x, pos_y, 25, color_idle);
    fill_circle(pos_x, pos_y, 6, set_gesture ? color_active : color_inactive);
    triang_h(pos_x - 20, pos_y,  7, (silent_direction == BGMT_SILENT_LEFT)  ? color_active : (silent_prev_direction == BGMT_SILENT_LEFT)  ? color_prev_active : color_inactive);
    triang_h(pos_x + 20, pos_y, -7, (silent_direction == BGMT_SILENT_RIGHT) ? color_active : (silent_prev_direction == BGMT_SILENT_RIGHT) ? color_prev_active : color_inactive);
    triang_v(pos_x, pos_y - 20,  7, (silent_direction == BGMT_SILENT_UP)    ? color_active : (silent_prev_direction == BGMT_SILENT_UP)    ? color_prev_active : color_inactive);
    triang_v(pos_x, pos_y + 20, -7, (silent_direction == BGMT_SILENT_DOWN)  ? color_active : (silent_prev_direction == BGMT_SILENT_DOWN)  ? color_prev_active : color_inactive);

    if (qm_gesture)
    {
        bmp_printf(
            SHADOW_FONT(FONT(FONT_MED, color_active, COLOR_BLACK)),
            pos_x - 20, pos_y - 25,
            gui_menu_shown() ? "Q" : "M"
        );
    }
}

static int translate_dir(int silent_dir, int prev_dir)
{
    switch (prev_dir)
    {
        case 0:
            /* simple direction press */
            switch (silent_dir)
            {
                case BGMT_SILENT_UP:
                    return BGMT_PRESS_UP;
                case BGMT_SILENT_DOWN:
                    return BGMT_PRESS_DOWN;
                case BGMT_SILENT_LEFT:
                    return BGMT_PRESS_LEFT;
                case BGMT_SILENT_RIGHT:
                    return BGMT_PRESS_RIGHT;
            }
            break;

        case BGMT_SILENT_UP:
            /* gesture: up->left or up->right */
            switch (silent_dir)
            {
                case BGMT_SILENT_LEFT:
                    return (gui_menu_shown()) ? BGMT_Q : BGMT_MENU;
            }
            break;

        case BGMT_SILENT_LEFT:
            /* gesture: up->left or up->right */
            switch (silent_dir)
            {
                case BGMT_SILENT_UP:
                    return (gui_menu_shown()) ? BGMT_Q : BGMT_MENU;
            }
            break;

        case BGMT_SILENT_RIGHT:
            /* gesture: right->up or right->down */
            switch (silent_dir)
            {
                case BGMT_SILENT_DOWN:
                    return BGMT_PRESS_SET;
            }
            break;

        case BGMT_SILENT_DOWN:
            /* gesture: down->left or down->right */
            switch (silent_dir)
            {
                case BGMT_SILENT_RIGHT:
                    return BGMT_PRESS_SET;
            }
            break;
    }
    return -1;
}

/* called from GUI timers */
static void silent_control_poll(int timer, void * unused)
{
    if (silent_pressed && !action_disabled())
    {
        silent_count++;
        delayed_call(20, silent_control_poll, 0);
    }
    else
    {
        if (silent_count > long_press_thr)
        {
            /* need to unpress? */
            fake_simple_button(BGMT_UNPRESS_UDLR);
        }
        /* erase the indicator and keep processing */
        redraw();
    }

    if (!gui_menu_shown())
    {
        silent_control_draw_indicator();
    }

    if (silent_count == long_press_thr)
    {
        /* long press */
        if (silent_direction && !silent_prev_direction)
        {
            fake_simple_button(translate_dir(silent_direction, 0));
        }

        /* make sure it won't re-trigger */
        silent_count++;
    }
    else if (silent_prev_direction && silent_count < long_press_thr)
    {
        /* gesture */
        fake_simple_button(translate_dir(silent_direction, silent_prev_direction));

        /* make sure it won't re-trigger */
        silent_count = long_press_thr + 1;
    }
}

int handle_silent_control_events(struct event * event)
{
    if (!silent_control_enabled)
    {
        return 1;
    }

    if (IS_FAKE(event))
    {
        return 1;
    }

    switch (event->param)
    {
        case BGMT_SILENT_UP:
        case BGMT_SILENT_DOWN:
        case BGMT_SILENT_RIGHT:
        case BGMT_SILENT_LEFT:
            silent_pressed = 1;
            silent_count = 0;
            silent_prev_direction = silent_direction;
            silent_direction = event->param;
            silent_control_draw_indicator();
            delayed_call(20, silent_control_poll, 0);
            info_led_on();
            return 0;

        case BGMT_SILENT_UNPRESS:
            silent_pressed = 0;
            silent_direction = 0;
            silent_prev_direction = 0;
            action_disabled_time = 0;
            info_led_off();
            return 0;

        //case BGMT_WHEEL_UP:       /* fixme: these don't work, why? */
        //case BGMT_WHEEL_DOWN:
        case BGMT_PRESS_SET:
        case BGMT_UNPRESS_SET:
            /* avoid false triggers when pressing SET */
            action_disabled_time = get_ms_clock();
            info_led_off();
            break;
    }

    return 1;
}

PROP_HANDLER(PROP_GUI_STATE)
{
    if (!silent_control_enabled)
    {
        return;
    }

    extern int ml_started;
    if (!ml_started) return;

    int gui_state = buf[0];

    /* doesn't work in idle mode anyway, but works in Canon menus */
    if (gui_state != GUISTATE_IDLE)
    {
        if (silent_control_status != 1)
        {
            silent_control_request();
        }
    }
}

static MENU_SELECT_FUNC(silent_control_toggle)
{
    silent_control_enabled = !silent_control_enabled;

    if (silent_control_enabled)
    {
        silent_control_request();

        /* reset state, just in case */
        silent_pressed = 0;
        silent_direction = 0;
        silent_prev_direction = 0;
        action_disabled_time = 0;
    }
    else
    {
        silent_control_release();
    }
}

static struct menu_entry silent_menu[] = {
    {
        .name   = "Silent Control",
        .priv   = &silent_control_enabled,
        .select = silent_control_toggle,
        .max    = 1,
        .help   = "Touch sensors on the rear scrollwheel, in both ML and Canon menus.",
        .help2  = "Long-press to navigate, top <-> left = Q/MENU, bottom <-> right = SET.",
    }
};

static void silent_init()
{
    menu_add("Misc key settings", silent_menu, COUNT(silent_menu));
}

INIT_FUNC(__FILE__, silent_init);
