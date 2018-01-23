/** \file
 * Patch the DlgLiveViewApp to hide Canon's bottom bar.
 */
#include "reloc.h"
#include "dryos.h"
#include "bmp.h"
#include "gui.h"
#include "dialog.h"
#include "config.h"
#include "zebra.h"
#include "lvinfo.h"
#include "propvalues.h"
#include "patch.h"

#ifdef CONFIG_LVAPP_HACK_PATCH

extern thunk LiveViewApp_handler_BL_JudgeBottomInfoDispTimerState;
static uint32_t addr = (uint32_t) &LiveViewApp_handler_BL_JudgeBottomInfoDispTimerState;
static int patched = 0;

static void liveviewapp_patch()
{
    if (!patched)
    {
        int err = patch_instruction(
            addr,
            MEM(addr),
            MOV_R0_0_INSTR,
            "Overlays: hide Canon bottom bar in LiveView"
        );
        if (!err) patched = 1;
    }
}

static void liveviewapp_unpatch()
{
    if (patched)
    {
        int err = unpatch_memory(addr);
        if (!err) patched = 0;
    }
}

extern void HideUnaviFeedBack_maybe();  /* Canon stub */

#endif

static int bottom_bar_dirty = 0;
int is_canon_bottom_bar_dirty() { return bottom_bar_dirty; }

/* note: this receives events other than button codes */
/* currently it doesn't do anything else, so it's placed here */
int handle_other_events(struct event * event)
{
    extern int ml_started;
    if (!ml_started) return 1;

#ifdef CONFIG_LVAPP_HACK_PATCH

    int should_hide =               /* hide Canon bottom bar: */
        lv &&                       /* in LiveView, */
        lv_disp_mode == 0 &&        /* if Canon overlays are hidden, */
        lv_dispsize == 1 &&         /* and zoom is x1, */
        get_global_draw_setting();  /* and ML overlays are enabled. */
    
    if (should_hide)
    {
        liveviewapp_patch();

        if (get_halfshutter_pressed())
        {
            bottom_bar_dirty = 10;
        }

        #ifdef UNAVI_FEEDBACK_TIMER_ACTIVE
        /*
         * Hide Canon's Q menu (aka UNAVI) as soon as the user quits it.
         * 
         * By default, this menu remains on screen for a few seconds.
         * After it disappears, we would have to redraw cropmarks, zebras and so on,
         * which looks pretty ugly, since our redraw is slow.
         * Better hide the menu right away, then redraw - it feels a lot less sluggish.
         */
        if (UNAVI_FEEDBACK_TIMER_ACTIVE)
        {
            HideUnaviFeedBack_maybe();
            bottom_bar_dirty = 0;
        }
        #endif
    }
    else
    {
        liveviewapp_unpatch();
        bottom_bar_dirty = 0;
    }

    unsigned short int lv_refreshing = lv && event->type == 2 && event->param == GMT_LOCAL_DIALOG_REFRESH_LV;
    if (lv_refreshing)
    {
        /* Redraw ML bottom bar if Canon bar was displayed over it */
        if (!liveview_display_idle()) bottom_bar_dirty = 0;
        if (bottom_bar_dirty) bottom_bar_dirty--;
        if (bottom_bar_dirty == 1)
        {
            lens_display_set_dirty();
        }
    }

#endif

    return 1;
}


// old code from Trammell's 1080i HDMI, good as documentation
/*
    // There are two add %pc that we can't fixup right now.
    // NOP the DebugMsg() calls that they would make
    *(uint32_t*) &reloc_buf[ 0xFFA97FAC + offset ] = NOP_INSTR;
    *(uint32_t*) &reloc_buf[ 0xFFA97F28 + offset ] = NOP_INSTR;

    // Fix up a few things, like the calls to ChangeHDMIOutputSizeToVGA
    // *(uint32_t*) &reloc_buf[ 0xFFA97C6C + offset ] = LOOP_INSTR;

    *(uint32_t*) &reloc_buf[ 0xFFA97D5C + offset ] = NOP_INSTR;
    *(uint32_t*) &reloc_buf[ 0xFFA97D60 + offset ] = NOP_INSTR;
    */
/*
    reloc_branch(
        (uintptr_t) &reloc_buf[ 0xFFA97D5C + offset ],
        //change_hdmi_size
        0xffa96260 // ChangeHDMIOutputToSizeToFULLHD
    );
*/

    //~ msleep( 4000 );
/*
    // Search the gui task list for the DlgLiveViewApp
    while(1)
    {
        msleep( 1000 );
        struct gui_task * current = gui_task_list.current;
        int y = 150;

        bmp_printf( FONT_SMALL, 400, y+=12,
            "current %08x",
            current
        );

        if( !current )
            continue;

        bmp_printf( FONT_SMALL, 400, y+=12,
            "handler %08x\npriv %08x",
            current->handler,
            current->priv
        );

        if( (void*) current->handler != (void*) dialog_handler )
            continue;

        struct dialog * dialog = current->priv;
        bmp_printf( FONT_SMALL, 400, y+=12,
            "dialog %08x",
            (unsigned) dialog->handler
        );

        if( dialog->handler == DlgLiveViewApp )
        {
            dialog->handler = (void*) new_DlgLiveViewApp;
            bmp_printf( FONT_SMALL, 400, y+=12, "new %08x", new_DlgLiveViewApp );
            bmp_hexdump( FONT_SMALL, 0, 300, new_DlgLiveViewApp, 128 );
            //bmp_hexdump( FONT_SMALL, 0, 300, reloc_buf, 128 );
        }
    }
}*/

//~ TASK_CREATE( __FILE__, reloc_dlgliveviewapp, 0, 0x1f, 0x1000 );

