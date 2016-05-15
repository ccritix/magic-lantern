/** \file
 * Replace the DlgLiveViewApp.
 *
 * Attempt to replace the DlgLiveViewApp with our own version.
 * Uses the reloc tools to do this.
 *
 */
#include "reloc.h"
#include "dryos.h"
#include "bmp.h"
#include "gui.h"
#include "dialog.h"
#include "config.h"
#include "patch.h"

extern thunk LiveViewApp_handler_BL_JudgeBottomInfoDispTimerState;
static uint32_t addr = (uint32_t) &LiveViewApp_handler_BL_JudgeBottomInfoDispTimerState;
static int patched = 0;

void reloc_liveviewapp_install()
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

void reloc_liveviewapp_uninstall()
{
    if (patched)
    {
        int err = unpatch_memory(addr);
        if (!err) patched = 0;
    }
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

