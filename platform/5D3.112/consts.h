#define CARD_DRIVE "A:/"
#define CARD_LED_ADDRESS 0xC022C06C // http://magiclantern.wikia.com/wiki/Led_addresses

// thanks Indy
#define HIJACK_INSTR_BL_CSTART  0xff0c0d7c
#define HIJACK_INSTR_BSS_END 0xff0c1cb8
#define HIJACK_FIXBR_BZERO32 0xff0c1c1c
#define HIJACK_FIXBR_CREATE_ITASK 0xff0c1ca8
#define HIJACK_INSTR_MY_ITASK 0xff0c1cc4
#define HIJACK_TASK_ADDR 0x23E14

// no idea if it's overflowing, need to check experimentally 
//~ #define ARMLIB_OVERFLOWING_BUFFER 0x21c94 // in AJ_armlib_setup_related3

#define DRYOS_ASSERT_HANDLER 0x23DF4 // dec TH_assert or assert_0

// Critical. Look for a call to prop_request_change(0x80050007, something, len).
#define AFFRAME_PROP_LEN 124
#define CUSTOM_WB_PROP_LEN 44

#define YUV422_LV_BUFFER_1 0x55207800 
#define YUV422_LV_BUFFER_2 0x55617800
#define YUV422_LV_BUFFER_3 0x55a27800

// http://magiclantern.wikia.com/wiki/VRAM_ADDR_from_code
// stateobj_disp[1]
#define YUV422_LV_BUFFER_DMA_ADDR (*(uint32_t*)(0x246a4+0x11c))

#define EVF_STATEOBJ *(struct state_object**)0x2600C)
#define YUV422_HD_BUFFER_DMA_ADDR 0x54000000


// http://magiclantern.wikia.com/wiki/ASM_Zedbra
#define YUV422_HD_BUFFER_1 0x54000000
#define YUV422_HD_BUFFER_2 0x4ee00000
//~ #define YUV422_HD_BUFFER_3 0x50000080
#define IS_HD_BUFFER(x)  ((0x400FFFFF & (x)) == 0x40000000 ) // quick check if x looks like a valid HD buffer

// see "focusinfo" and Wiki:Struct_Guessing
#define FOCUS_CONFIRMATION (*(int*)0x276D0)

// See "cam event metering"
#define HALFSHUTTER_PRESSED (*(int*)0x251D4)

#define DISPLAY_SENSOR_POWERED 0

#define GMT_IDLEHANDLER_TASK (*(int*)0x2e81c) // dec create_idleHandler_task

// for gui_main_task
#define GMT_NFUNCS 7
#define GMT_FUNCTABLE 0xff796d4c // dec gui_main_task
//#define GMT_IDLEHANDLER_TASK (*(int*)0x2e81c) // dec create_idleHandler_task

// button codes as received by gui_main_task
// need to print those on screen
#define BGMT_WHEEL_UP 0
#define BGMT_WHEEL_DOWN 1
#define BGMT_WHEEL_LEFT 2
#define BGMT_WHEEL_RIGHT 3

#define BGMT_PRESS_SET 4
#define BGMT_UNPRESS_SET 5

#define BGMT_MENU 6
#define BGMT_INFO 7
#define BGMT_PLAY 0xB // ?!
#define BGMT_TRASH 0xD

#define BGMT_PRESS_DP 0x2f
#define BGMT_UNPRESS_DP 0x35
#define BGMT_RATE 0x21
#define BGMT_REC 0x1E


#define BGMT_PRESS_ZOOMIN_MAYBE 0x12
#define BGMT_UNPRESS_ZOOMIN_MAYBE 0x13
//~ #define BGMT_PRESS_ZOOMOUT_MAYBE 0x1234 // no zoom out button in play mode?!
//~ #define BGMT_UNPRESS_ZOOMOUT_MAYBE 0x5678

#define BGMT_LV 0x1E
#define BGMT_Q 0x1d
//~ #define BGMT_Q_ALT 0x67

//~ #define BGMT_FUNC 0x12
#define BGMT_PICSTYLE 0x13
//~ #define BGMT_JOY_CENTER (lv ? 0x1e : 0x3b)
#define BGMT_JOY_CENTER 0x3e

#define BGMT_PRESS_UP 0x36
#define BGMT_PRESS_UP_RIGHT 0x17
#define BGMT_PRESS_UP_LEFT 0x18
#define BGMT_PRESS_RIGHT 0x39
#define BGMT_PRESS_LEFT 0x3a
#define BGMT_PRESS_DOWN_RIGHT 0x1B
#define BGMT_PRESS_DOWN_LEFT 0x1C
#define BGMT_PRESS_DOWN 0x3d

#define BGMT_UNPRESS_UDLR 0x35
#define BGMT_PRESS_HALFSHUTTER 0x4e
#define BGMT_UNPRESS_HALFSHUTTER 0x4f
#define BGMT_PRESS_FULLSHUTTER 0x50
#define BGMT_UNPRESS_FULLSHUTTER 0x51

#define BGMT_FLASH_MOVIE 0
#define BGMT_PRESS_FLASH_MOVIE 0
#define BGMT_UNPRESS_FLASH_MOVIE 0
#define FLASH_BTN_MOVIE_MODE 0
#define BGMT_ISO_MOVIE 0
#define BGMT_PRESS_ISO_MOVIE 0
#define BGMT_UNPRESS_ISO_MOVIE 0

#define GMT_GUICMD_PRESS_BUTTON_SOMETHING 0x52 // unhandled buttons?

#define BGMT_LIGHT 0x20 // the little button for top screen backlight

#define GMT_OLC_INFO_CHANGED 103 // backtrace copyOlcDataToStorage call in gui_massive_event_loop

// needed for correct shutdown from powersave modes
#define GMT_GUICMD_START_AS_CHECK 95
#define GMT_GUICMD_OPEN_SLOT_COVER 91
#define GMT_GUICMD_LOCK_OFF 89


 #define SENSOR_RES_X 4752
 #define SENSOR_RES_Y 3168

#define LV_BOTTOM_BAR_DISPLAYED (((*(int*)0x29754) == 0xF))
#define ISO_ADJUSTMENT_ACTIVE ((*(int*)0x29754) == 0xF) // dec ptpNotifyOlcInfoChanged and look for: if arg1 == 1: MEM(0x79B8) = *(arg2)

// from a screenshot
#define COLOR_FG_NONLV 1

#define MVR_516_STRUCT (*(void**)0x241A0) // look in MVR_Initialize for AllocateMemory call; decompile it and see where ret_AllocateMemory is stored.

#define MEM(x) (*(int*)(x))
#define div_maybe(a,b) ((a)/(b))

// see mvrGetBufferUsage, which is not really safe to call => err70
// macros copied from arm-console
#define MVR_BUFFER_USAGE_FRAME ABS(div_maybe(-MEM(832 + MVR_516_STRUCT) + MEM(500 + MVR_516_STRUCT), MEM(668 + MVR_516_STRUCT)))
#define MVR_BUFFER_USAGE_SOUND div_maybe(-100*MEM(708 + MVR_516_STRUCT) + 100*MEM(696 + MVR_516_STRUCT), 0xa)
#define MVR_BUFFER_USAGE MAX(MVR_BUFFER_USAGE_FRAME, MVR_BUFFER_USAGE_SOUND)

#define MVR_FRAME_NUMBER  (*(int*)(0x1F4 + MVR_516_STRUCT)) // in mvrExpStarted
#define MVR_BYTES_WRITTEN 0 // idk; 5D2: (*(int*)(0xE4 + MVR_516_STRUCT)) // in mvrSMEncodeDone

#define MOV_RES_AND_FPS_COMBINATIONS 5 // 3 fullhd, 2 hd, not changing the two VGA modes; worth trying with 9
#define MOV_OPT_NUM_PARAMS 2
#define MOV_GOP_OPT_NUM_PARAMS 5
#define MOV_OPT_STEP 5
#define MOV_GOP_OPT_STEP 5

#define AE_VALUE 0 // 404

#define CURRENT_DIALOG_MAYBE (*(int*)0x26634) // not sure

#define DLG_PLAY 1
#define DLG_MENU 2

// not sure
#define DLG_FOCUS_MODE 0x123456
//~ #define DLG_DRIVE_MODE 8
//~ #define DLG_PICTURE_STYLE 4
//~ #define DLG_Q_UNAVI 0x18
//~ #define DLG_FLASH_AE 0x22
//~ #define DLG_PICQ 6

#define _MOVIE_MODE_NON_LIVEVIEW 0 //(!lv && !lv_paused && !get_lv_stopped_by_user() && gui_state == GUISTATE_IDLE && lv_movie_select == LVMS_ENABLE_MOVIE && lens_info.job_state == 0 && !HALFSHUTTER_PRESSED)
#define DLG_MOVIE_ENSURE_A_LENS_IS_ATTACHED 0//  (_MOVIE_MODE_NON_LIVEVIEW && !lens_info.name[0])
#define DLG_MOVIE_PRESS_LV_TO_RESUME 0//(_MOVIE_MODE_NON_LIVEVIEW && lens_info.name[0])



#define PLAY_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_DIALOG_MAYBE == DLG_PLAY)
#define MENU_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_DIALOG_MAYBE == DLG_MENU)

//~ #define AUDIO_MONITORING_HEADPHONES_CONNECTED (!((*(int*)0xc0220070) & 1))
//~ #define HOTPLUG_VIDEO_OUT_PROP_DELIVER_ADDR 0x1aac // this prop_deliver performs the action for Video Connect and Video Disconnect
//~ #define HOTPLUG_VIDEO_OUT_STATUS_ADDR 0x1ad4 // passed as 2nd arg to prop_deliver; 1 = display connected, 0 = not, other values disable this event (trick)

// In bindGUIEventFromGUICBR, look for "LV Set" => arg0 = 8
// Next, in SetGUIRequestMode, look at what code calls NotifyGUIEvent(8, something)
#define GUIMODE_ML_MENU (recording ? 0 : lv ? 86 : 2)

// position for displaying clock outside LV
#define DISPLAY_CLOCK_POS_X 435
#define DISPLAY_CLOCK_POS_Y 452

#define MENU_DISP_ISO_POS_X 500
#define MENU_DISP_ISO_POS_Y 27

// for displaying TRAP FOCUS msg outside LV
#define DISPLAY_TRAP_FOCUS_POS_X 500
#define DISPLAY_TRAP_FOCUS_POS_Y 320
#define DISPLAY_TRAP_FOCUS_MSG       "TRAP \nFOCUS"
#define DISPLAY_TRAP_FOCUS_MSG_BLANK "     \n     "

#define NUM_PICSTYLES 9
#define PROP_PICSTYLE_SETTINGS(i) (PROP_PICSTYLE_SETTINGS_STANDARD - 1 + i)

#define MOVIE_MODE_REMAP_X SHOOTMODE_ADEP
#define MOVIE_MODE_REMAP_Y SHOOTMODE_CA
#define MOVIE_MODE_REMAP_X_STR "A-DEP"
#define MOVIE_MODE_REMAP_Y_STR "CA"

#define FLASH_MAX_EV 3
#define FLASH_MIN_EV -10 // not sure if it actually works
#define FASTEST_SHUTTER_SPEED_RAW 160
#define MAX_AE_EV 2

#define DIALOG_MnCardFormatBegin (0x363BC) // ret_CreateDialogBox(...DlgMnCardFormatBegin_handler...) is stored there
#define DIALOG_MnCardFormatExecute (0x39B98) // similar

#define BULB_MIN_EXPOSURE 100

// http://magiclantern.wikia.com/wiki/Fonts
#define BFNT_CHAR_CODES    0xf7363764
#define BFNT_BITMAP_OFFSET 0xf7366868
#define BFNT_BITMAP_DATA   0xf736996c

#define DLG_SIGNATURE 0x6E4944

// from CFn
 #define AF_BTN_HALFSHUTTER 0
 #define AF_BTN_STAR 2

#define IMGPLAY_ZOOM_LEVEL_ADDR (0x2E9C4) // dec GuiImageZoomDown and look for a negative counter
#define IMGPLAY_ZOOM_LEVEL_MAX 14

#define BULB_EXPOSURE_CORRECTION 150 // min value for which bulb exif is OK [not tested]

// see http://magiclantern.wikia.com/wiki/VRAM/BMP
#define WINSYS_BMP_DIRTY_BIT_NEG MEM(0x323b0+0x2c)

#define BTN_ZEBRAS_FOR_PLAYBACK BGMT_LIGHT // what button to use for zebras in Play mode

// manual exposure overrides
#define LVAE_STRUCT 0x68BB8
#define CONTROL_BV      (*(uint16_t*)(LVAE_STRUCT+0x20)) // EP_SetControlBv
#define CONTROL_BV_TV   (*(uint16_t*)(LVAE_STRUCT+0x22)) // EP_SetControlParam
#define CONTROL_BV_AV   (*(uint16_t*)(LVAE_STRUCT+0x24))
#define CONTROL_BV_ISO  (*(uint16_t*)(LVAE_STRUCT+0x26))
#define CONTROL_BV_ZERO (*(uint16_t*)(LVAE_STRUCT+0x28))
#define LVAE_ISO_SPEED  (*(uint8_t* )(LVAE_STRUCT))      // offset 0x0; at 3 it changes iso very slowly
//~ #define LVAE_ISO_MIN    (*(uint8_t* )(LVAE_STRUCT+0x28)) // string: ISOMin:%d
//~ #define LVAE_ISO_HIS    (*(uint8_t* )(LVAE_STRUCT+0x2a)) // no idea what this is
#define LVAE_DISP_GAIN  (*(uint16_t*)(LVAE_STRUCT+0x3c)) // lvae_setdispgain
#define LVAE_MOV_M_CTRL (*(uint8_t* )(LVAE_STRUCT+0x1c)) // lvae_setmoviemanualcontrol

#define MIN_MSLEEP 10

#define INFO_BTN_NAME "INFO"
#define Q_BTN_NAME "[Q]"
#define ARROW_MODE_TOGGLE_KEY "PicStyle"

#define DISPLAY_STATEOBJ (*(struct state_object **)0x246a4)
#define DISPLAY_IS_ON (DISPLAY_STATEOBJ->current_state != 0)

#define VIDEO_PARAMETERS_SRC_3 MEM(0x25FF0) //for mark iii
#define FRAME_ISO (*(uint16_t*)(VIDEO_PARAMETERS_SRC_3+0xc)) // for sure now

//~ #define LV_STRUCT_PTR 0x1D78
//~ #define FRAME_ISO *(uint16_t*)(MEM(LV_STRUCT_PTR) + 0x5C)

// see "Malloc Information"
#define MALLOC_STRUCT 0x3c268
#define MALLOC_FREE_MEMORY (MEM(MALLOC_STRUCT + 8) - MEM(MALLOC_STRUCT + 0x1C)) // "Total Size" - "Allocated Size"
