/*
 *  Almost none of this is correct yet, only a skeleton to be filled in later.
 *
 *  Indented line = incorrect.
 */

#define CARD_DRIVE "B:/"
#define CARD_LED_ADDRESS 0xC022C184 // http://magiclantern.wikia.com/wiki/Led_addresses
#define LEDON 0x138800
#define LEDOFF 0x838C00

#define HIJACK_CACHE_HACK

#define HIJACK_CACHE_HACK_INITTASK_ADDR 0xFF0C1C6C

// load ML in the malloc pool
//~ #define HIJACK_CACHE_HACK_BSS_END_ADDR 0xFF0C1C64
//~ #define HIJACK_CACHE_HACK_BSS_END_INSTR 0xD7900

// load ML in the AllocateMemory pool
#define HIJACK_CACHE_HACK_BSS_END_ADDR 0xff0c3470
#define HIJACK_CACHE_HACK_BSS_END_INSTR 0xCBC000

#define HIJACK_INSTR_BL_CSTART  0xFF0C0D90
#define HIJACK_INSTR_BSS_END 0xFF0C1C64
#define HIJACK_FIXBR_BZERO32 0xFF0C1BB8
#define HIJACK_FIXBR_CREATE_ITASK 0xFF0C1C54
#define HIJACK_INSTR_MY_ITASK 0xFF0C1C6C
#define HIJACK_TASK_ADDR 0x74BD8

#define CACHE_HACK_FLUSH_RATE_SLAVE 0xFF1ABEC0

// look for LDRNE near 2nd ARM Library runtime error
#define ARMLIB_OVERFLOWING_BUFFER 0x93b58 // in AJ_armlib_setup_related3

#define DRYOS_ASSERT_HANDLER 0x74BB8 // dec TH_assert or assert_0

#define YUV422_LV_BUFFER_1 0x5F227800
#define YUV422_LV_BUFFER_2 0x5F637800
#define YUV422_LV_BUFFER_3 0x5EE17800

// http://magiclantern.wikia.com/wiki/VRAM_ADDR_from_code
// stateobj_disp[1]
#define YUV422_LV_BUFFER_DISPLAY_ADDR (*(uint32_t*)(0x754BC+0xA4))

#define REG_EDMAC_WRITE_LV_ADDR 0xc0f04008 // SDRAM address of LV buffer (aka VRAM)
#define REG_EDMAC_WRITE_HD_ADDR 0xc0f04a08 // SDRAM address of HD buffer (aka YUV)

#define YUV422_HD_BUFFER_DMA_ADDR (shamem_read(REG_EDMAC_WRITE_HD_ADDR) + vram_hd.pitch) // first line from DMA is dummy


// http://magiclantern.wikia.com/wiki/ASM_Zedbra
#define YUV422_HD_BUFFER_1 0x13FFF780
#define YUV422_HD_BUFFER_2 0x0EFFF780


// see "focusinfo" and Wiki:Struct_Guessing
#define FOCUS_CONFIRMATION (*(int*)0x78664)

//~ look for string "[MC] permit LV instant", it's the struct refrenced in this function.
#define HALFSHUTTER_PRESSED (*(int*)0x75FCC)

    #define DISPLAY_SENSOR_POWERED 0

// for gui_main_task
#define GMT_NFUNCS 7
#define GMT_FUNCTABLE 0xFF9CDB68 // dec gui_main_task

#define SENSOR_RES_X 4344
#define SENSOR_RES_Y 2611

#define CURRENT_DIALOG_MAYBE (*(int*)0x77638)

    #define LV_BOTTOM_BAR_DISPLAYED (lv_disp_mode)

//That Function is dead.
#define ISO_ADJUSTMENT_ACTIVE 0
//#define ISO_ADJUSTMENT_ACTIVE 0x7AAD0 // dec ptpNotifyOlcInfoChanged and look for: if arg1 == 1: MEM(0x79B8) = *(arg2)

// from a screenshot
#define COLOR_FG_NONLV 1

#define MVR_516_STRUCT (*(void**)0x74FA0) // look in MVR_Initialize for AllocateMemory call; decompile it and see where ret_AllocateMemory is stored.

#define MEM(x) (*(volatile int*)(x))
#define div_maybe(a,b) ((a)/(b))

// see mvrGetBufferUsage, which is not really safe to call => err70
// macros copied from arm-console
#define MVR_BUFFER_USAGE_FRAME MAX(MEM(MVR_516_STRUCT + 0x594), MEM(MVR_516_STRUCT + 0x590))
    #define MVR_BUFFER_USAGE_SOUND 0 // not sure
#define MVR_BUFFER_USAGE MAX(MVR_BUFFER_USAGE_FRAME, MVR_BUFFER_USAGE_SOUND)

#define MVR_FRAME_NUMBER  (*(int*)(0x1FC + MVR_516_STRUCT)) // in mvrExpStarted
    #define MVR_BYTES_WRITTEN (*(int*)(0xb0 + MVR_516_STRUCT))
        #define AE_VALUE 0 // 404

#define DLG_PLAY 1
#define DLG_MENU 2

        #define DLG_FOCUS_MODE 0x123456

#define DLG_MOVIE_ENSURE_A_LENS_IS_ATTACHED (CURRENT_DIALOG_MAYBE == 0x24)
#define DLG_MOVIE_PRESS_LV_TO_RESUME (CURRENT_DIALOG_MAYBE == 0x25)

#define PLAY_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_DIALOG_MAYBE == DLG_PLAY)
#define MENU_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_DIALOG_MAYBE == DLG_MENU)

#define AUDIO_MONITORING_HEADPHONES_CONNECTED (!((*(int*)0xC0220174) & 1)) //NE((*0xC0220174 & 0x1)):
#define HOTPLUG_VIDEO_OUT_PROP_DELIVER_ADDR 0x74C44 
#define HOTPLUG_VIDEO_OUT_STATUS_ADDR 0x74c34 //prop_deliver(*0x74C44, 0x74c34, 0x4, 0x0) +*0x74C34 = 1
// In bindGUIEventFromGUICBR, look for "LV Set" => arg0 = 8
// Next, in SetGUIRequestMode, look at what code calls NotifyGUIEvent(8, something)

// In bindGUIEventFromGUICBR, look for "LV Set" => arg0 = 8
// Next, in SetGUIRequestMode, look at what code calls NotifyGUIEvent(8, something)
#define GUIMODE_ML_MENU (recording ? 0 : lv ? 92 : 2) // any from 90...102 ?! find one that works (trial/error)

// position for displaying clock outside LV
#define DISPLAY_CLOCK_POS_X 435
#define DISPLAY_CLOCK_POS_Y 452

#define MENU_DISP_ISO_POS_X 500
#define MENU_DISP_ISO_POS_Y 27

// for displaying battery
#define DISPLAY_BATTERY_POS_X 149
#define DISPLAY_BATTERY_POS_Y 410
#define DISPLAY_BATTERY_LEVEL_1 60
#define DISPLAY_BATTERY_LEVEL_2 20

//for HTP mode on display
    #define HTP_STATUS_POS_X 500
    #define HTP_STATUS_POS_Y 233

// for HDR status
    #define HDR_STATUS_POS_X 180
    #define HDR_STATUS_POS_Y 460

//for Mirror Lock Up enabled on display
#define MLU_STATUS_POS_X 335
#define MLU_STATUS_POS_Y 365

// for the yellow ISO range [a-b]
#define ISO_RANGE_POS_X 545
#define ISO_RANGE_POS_Y 105

#define WB_KELVIN_POS_X 190
#define WB_KELVIN_POS_Y 280

// white balance shift values M2B1 in yellow
#define WBS_POS_X 265
#define WBS_POS_Y 278

// for header footer info
#define DISPLAY_HEADER_FOOTER_INFO

// for displaying TRAP FOCUS msg outside LV
    #define DISPLAY_TRAP_FOCUS_POS_X 50
    #define DISPLAY_TRAP_FOCUS_POS_Y 360
    #define DISPLAY_TRAP_FOCUS_MSG       "TRAP FOCUS"
    #define DISPLAY_TRAP_FOCUS_MSG_BLANK "          "

#define NUM_PICSTYLES 10
#define PROP_PICSTYLE_SETTINGS(i) ((i) == 1 ? PROP_PICSTYLE_SETTINGS_AUTO : PROP_PICSTYLE_SETTINGS_STANDARD - 2 + i)

    #define FLASH_MAX_EV 3
    #define FLASH_MIN_EV -10 // not sure if it actually works
// 1/8000+ Possible but canon keeps resetting it.
//#define FASTEST_SHUTTER_SPEED_RAW 160
#define FASTEST_SHUTTER_SPEED_RAW 152
    #define MAX_AE_EV 5

#define DIALOG_MnCardFormatBegin (0x8888C) // ret_CreateDialogBox(...DlgMnCardFormatBegin_handler...) is stored there
#define DIALOG_MnCardFormatExecute (0x8DAF0) // similar

#define BULB_MIN_EXPOSURE 500

// http://magiclantern.wikia.com/wiki/Fonts
#define BFNT_CHAR_CODES    0xf03664d0
#define BFNT_BITMAP_OFFSET 0xf0369794
#define BFNT_BITMAP_DATA   0xf036ca58


#define DLG_SIGNATURE 0x6e6144  //~ look in stubs api stability test log: [Pass] MEM(dialog->type) => 0x6e6144

    // from CFn
         #define AF_BTN_HALFSHUTTER 0
         #define AF_BTN_STAR 2

#define IMGPLAY_ZOOM_LEVEL_ADDR (0x7F77C) // dec GuiImageZoomDown and look for a negative counter
#define IMGPLAY_ZOOM_LEVEL_MAX 14
#define IMGPLAY_ZOOM_POS_X MEM(0xb9a38) // CentrePos
#define IMGPLAY_ZOOM_POS_Y MEM(0xb9a3C) // '[ImgPlyer] ScrollWidth:%ld ScrollHeight:%ld'

    #define IMGPLAY_ZOOM_POS_X_CENTER 360
    #define IMGPLAY_ZOOM_POS_Y_CENTER 240
#define IMGPLAY_ZOOM_POS_DELTA_X 110 //(0x2be - 0x190)
#define IMGPLAY_ZOOM_POS_DELTA_Y 90 //(0x1d4 - 0x150)

    #define BULB_EXPOSURE_CORRECTION 650 // looks huge...

// see http://magiclantern.wikia.com/wiki/VRAM/BMP
#define WINSYS_BMP_DIRTY_BIT_NEG MEM(0x82B24)   //~ from string: refresh partly

// manual exposure overrides
#define LVAE_STRUCT 0xC4D78
#define CONTROL_BV      (*(uint16_t*)(LVAE_STRUCT+0x20)) // EP_SetControlBv
#define CONTROL_BV_TV   (*(uint16_t*)(LVAE_STRUCT+0x22)) // EP_SetControlParam
#define CONTROL_BV_AV   (*(uint16_t*)(LVAE_STRUCT+0x24))
#define CONTROL_BV_ISO  (*(uint16_t*)(LVAE_STRUCT+0x26))
#define CONTROL_BV_ZERO (*(uint16_t*)(LVAE_STRUCT+0x28))
#define LVAE_ISO_SPEED  (*(uint8_t* )(LVAE_STRUCT))      // offset 0x0; at 3 it changes iso very slowly
//~     #define LVAE_ISO_MIN    (*(uint8_t* )(LVAE_STRUCT+0x28)) // string: ISOMin:%d
//~     #define LVAE_ISO_HIS    (*(uint8_t* )(LVAE_STRUCT+0x2a)) // no idea what this is
#define LVAE_DISP_GAIN  (*(uint16_t*)(LVAE_STRUCT+0x3c)) // lvae_setdispgain
#define LVAE_MOV_M_CTRL (*(uint8_t* )(LVAE_STRUCT+0x1c)) // lvae_setmoviemanualcontrol

#define MIN_MSLEEP 10

#define INFO_BTN_NAME "INFO"
#define Q_BTN_NAME "[Q]"
#define ARROW_MODE_TOGGLE_KEY "Foc Pnts"

#define DISPLAY_STATEOBJ (*(struct state_object **)0x75550)
#define DISPLAY_IS_ON (DISPLAY_STATEOBJ->current_state != 0)

#define VIDEO_PARAMETERS_SRC_3 MEM(0x76cfc) //76cfc
#define FRAME_ISO (*(uint8_t*)(VIDEO_PARAMETERS_SRC_3+0))
#define FRAME_APERTURE (*(uint8_t*)(VIDEO_PARAMETERS_SRC_3+1))
#define FRAME_SHUTTER (*(uint8_t*)(VIDEO_PARAMETERS_SRC_3+2))
#define FRAME_SHUTTER_TIMER (*(uint16_t*)(VIDEO_PARAMETERS_SRC_3+6))
#define FRAME_BV ((int)FRAME_SHUTTER + (int)FRAME_APERTURE - (int)FRAME_ISO)

// see "Malloc Information"
#define MALLOC_STRUCT 0x94818
#define MALLOC_FREE_MEMORY (MEM(MALLOC_STRUCT + 8) - MEM(MALLOC_STRUCT + 0x1C)) // "Total Size" - "Allocated Size"

//~ needs fixed to prevent half shutter making canon overlays visible. sub_ff52c568.htm Not Present but probably right.
#define UNAVI_FEEDBACK_TIMER_ACTIVE (MEM(0x84100) != 0x17) // dec CancelUnaviFeedBackTimer

