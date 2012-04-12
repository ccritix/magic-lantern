#define CARD_DRIVE "B:/"
#define CARD_LED_ADDRESS 0xC0220134 // http://magiclantern.wikia.com/wiki/Led_addresses

#define HIJACK_INSTR_BL_CSTART 0xFF012AE8
#define HIJACK_INSTR_BSS_END 0xFF01093C
#define HIJACK_FIXBR_BZERO32 0xFF0108A4
#define HIJACK_FIXBR_CREATE_ITASK 0xFF01092C
#define HIJACK_INSTR_MY_ITASK 0xFF010948
#define HIJACK_TASK_ADDR 0x1A74

#define ARMLIB_OVERFLOWING_BUFFER 0x244c0 // in AJ_armlib_setup_related3

#define DRYOS_ASSERT_HANDLER 0x1A18 // dec TH_assert or assert_0

// 720x480, changes when external monitor is connected
#define YUV422_LV_BUFFER_1 0x41B07800
#define YUV422_LV_BUFFER_2 0x43738800
#define YUV422_LV_BUFFER_3 0x43B48800
#define YUV422_LV_PITCH 1440
//~ #define YUV422_LV_PITCH_RCA 1080
//~ #define YUV422_LV_PITCH_HDMI 3840
//~ #define YUV422_LV_HEIGHT 480
//~ #define YUV422_LV_HEIGHT_RCA 540
//~ #define YUV422_LV_HEIGHT_HDMI 1080

#define YUV422_LV_BUFFER_DMA_ADDR (*(uint32_t*)0x26A8) // workaround
//#define YUV422_LV_BUFFER_DMA_ANOTHER_ADDR (*(uint32_t*)0x4c60)
#define YUV422_HD_BUFFER_DMA_ADDR (*(uint32_t*)0x48a0)


#define YUV422_HD_BUFFER_1 0x44000080
#define YUV422_HD_BUFFER_2 0x46000080
#define IS_HD_BUFFER(x)  ((0x40FFFFFF & (x)) == 0x40000080 ) // quick check if x looks like a valid HD buffer

/*#define YUV422_HD_PITCH_IDLE 2112
 #define YUV422_HD_HEIGHT_IDLE 704
 
 #define YUV422_HD_PITCH_ZOOM 2048
 #define YUV422_HD_HEIGHT_ZOOM 680
 
 #define YUV422_HD_PITCH_REC_FULLHD 3440
 #define YUV422_HD_HEIGHT_REC_FULLHD 974
 
 // guess
 #define YUV422_HD_PITCH_REC_720P 2560
 #define YUV422_HD_HEIGHT_REC_720P 580
 
 #define YUV422_HD_PITCH_REC_480P 1280
 #define YUV422_HD_HEIGHT_REC_480P 480*/

#define FOCUS_CONFIRMATION (*(int*)0x3edc) // see "focusinfo" and Wiki:Struct_Guessing
#define HALFSHUTTER_PRESSED (*(int*)0x1b74) // used for Trap Focus and Magic Off.
//~ #define AF_BUTTON_PRESSED_LV 0
// To find it, go to MainCtrl task and take the number from the second line minus 4.
// See also "cam event metering"

//~ #define DISPLAY_SENSOR (*(int*)0x2dec)
//~ #define DISPLAY_SENSOR_ACTIVE (*(int*)0xC0220104)
#define DISPLAY_SENSOR_POWERED (*(int*)0x328c) // dec AJ_Req_DispSensorStart

// button codes as received by gui_main_task
#define BGMT_PRESS_LEFT 0x39
#define BGMT_UNPRESS_LEFT 0x3a
#define BGMT_PRESS_UP 0x3b
#define BGMT_UNPRESS_UP 0x3c
#define BGMT_PRESS_RIGHT 0x37
#define BGMT_UNPRESS_RIGHT 0x38
#define BGMT_PRESS_DOWN 0x3d
#define BGMT_UNPRESS_DOWN 0x3e
#define BGMT_PRESS_SET 4
#define BGMT_UNPRESS_SET 5
#define BGMT_TRASH 0xA
#define BGMT_MENU 6
#define BGMT_INFO 7
#define BGMT_Q 0xF
#define BGMT_Q_ALT 0xF
#define BGMT_PLAY 9
#define BGMT_PRESS_HALFSHUTTER 0x23
#define BGMT_UNPRESS_HALFSHUTTER 0x24
#define BGMT_PRESS_FULLSHUTTER 0x25
#define BGMT_UNPRESS_FULLSHUTTER 0x26
#define BGMT_PRESS_ZOOMIN_MAYBE 0xB
#define BGMT_UNPRESS_ZOOMIN_MAYBE 0xC
#define BGMT_PRESS_ZOOMOUT_MAYBE 0xD
#define BGMT_UNPRESS_ZOOMOUT_MAYBE 0xE
#define BGMT_JOY_CENTER 0x1e // press the joystick maybe?		???

#define BGMT_LV 0xf						// idk?

#define BGMT_WHEEL_LEFT 2
#define BGMT_WHEEL_RIGHT 3
#define BGMT_WHEEL_UP 0
#define BGMT_WHEEL_DOWN 1

/*#define BGMT_FLASH_MOVIE (event->type == 0 && event->param == 0x3f && shooting_mode == SHOOTMODE_MOVIE && event->arg == 0x9)
#define BGMT_PRESS_FLASH_MOVIE (BGMT_FLASH_MOVIE && (*(int*)(event->obj) & 0x1000000))
#define BGMT_UNPRESS_FLASH_MOVIE (BGMT_FLASH_MOVIE && (*(int*)(event->obj) & 0x1000000) == 0)
#define FLASH_BTN_MOVIE_MODE get_flash_movie_pressed()

#define BGMT_ISO_MOVIE (event->type == 0 && event->param == 0x56 && shooting_mode == SHOOTMODE_MOVIE && event->arg == 0x9)
#define BGMT_PRESS_ISO_MOVIE (BGMT_ISO_MOVIE && (*(int*)(event->obj) & 0x1000000))
#define BGMT_UNPRESS_ISO_MOVIE (BGMT_ISO_MOVIE && (*(int*)(event->obj) & 0x1000000) == 0)*/

#define BGMT_FLASH_MOVIE 0
#define BGMT_PRESS_FLASH_MOVIE 0
#define BGMT_UNPRESS_FLASH_MOVIE 0
#define FLASH_BTN_MOVIE_MODE 0

#define BGMT_ISO_MOVIE 0
#define BGMT_PRESS_ISO_MOVIE 0
#define BGMT_UNPRESS_ISO_MOVIE 0

#define LV_BOTTOM_BAR_DISPLAYED (MEM(0x329D8) != 3)



#define GMT_OLC_INFO_CHANGED 63 // backtrace copyOlcDataToStorage call in gui_massive_event_loop

#define SENSOR_RES_X 4752
#define SENSOR_RES_Y 3168

//~ #define FLASH_BTN_MOVIE_MODE (((*(int*)0x14c1c) & 0x40000) && (shooting_mode == SHOOTMODE_MOVIE))
//~ #define CLK_25FPS 0x1e24c  // this is updated at 25fps and seems to be related to auto exposure

//~ #define AJ_LCD_Palette 0x2CDB0


#define ISO_ADJUSTMENT_ACTIVE ((*(int*)0x77BC) == 0xF)
#define SHOOTING_MODE (*(int*)0x313C)

#define COLOR_FG_NONLV 80

#define MVR_190_STRUCT (*(void**)0x1e90) // look in MVR_Initialize for AllocateMemory call; decompile it and see where ret_AllocateMemory is stored.

#define MEM(x) (*(int*)(x))
#define div_maybe(a,b) ((a)/(b))

// see mvrGetBufferUsage, which is not really safe to call => err70
// macros copied from arm-console
#define MVR_BUFFER_USAGE div_maybe(-100*MEM(316 + MVR_190_STRUCT) - 100*MEM(324 + MVR_190_STRUCT) - 100*MEM(496 + MVR_190_STRUCT) - 100*MEM(504 + MVR_190_STRUCT) + 100*MEM(320 + MVR_190_STRUCT) + 100*MEM(328 + MVR_190_STRUCT), -MEM(316 + MVR_190_STRUCT) - MEM(324 + MVR_190_STRUCT) + MEM(320 + MVR_190_STRUCT) + MEM(328 + MVR_190_STRUCT))
#define MVR_FRAME_NUMBER (*(int*)(300 + MVR_190_STRUCT))
//#define MVR_LAST_FRAME_SIZE (*(int*)(512 + MVR_190_STRUCT))
#define MVR_BYTES_WRITTEN (*(int*)(292 + MVR_190_STRUCT))

#define MOV_RES_AND_FPS_COMBINATIONS 3
#define MOV_OPT_NUM_PARAMS 2
#define MOV_GOP_OPT_NUM_PARAMS 3
#define MOV_OPT_STEP 5
#define MOV_GOP_OPT_STEP 3

#define AE_VALUE (*(int8_t*)0x14E20)

#define CURRENT_DIALOG_MAYBE (*(int*)0x3a9c)
#define DLG_WB 0x24
#define DLG_FOCUS_MODE 0x27
#define DLG_DRIVE_MODE 0x28
#define DLG_PICTURE_STYLE 0x23
#define DLG_PLAY 1
#define DLG_MENU 2
#define DLG_Q_UNAVI 0x1E
#define DLG_FLASH_AE 0x21
#define DLG_PICQ 0x26
#define DLG_MOVIE_ENSURE_A_LENS_IS_ATTACHED (CURRENT_DIALOG_MAYBE == 0x1B)
#define DLG_MOVIE_PRESS_LV_TO_RESUME (CURRENT_DIALOG_MAYBE == 0x1B)

#define AUDIO_MONITORING_HEADPHONES_CONNECTED (!((*(int*)0xc0220070) & 1))
#define HOTPLUG_VIDEO_OUT_PROP_DELIVER_ADDR 0x1afc // this prop_deliver performs the action for Video Connect and Video Disconnect
#define HOTPLUG_VIDEO_OUT_STATUS_ADDR 0x1b20 // passed as 2nd arg to prop_deliver; 1 = display connected, 0 = not, other values disable this event (trick)

#define PLAY_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_DIALOG_MAYBE == DLG_PLAY)
#define MENU_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_DIALOG_MAYBE == DLG_MENU)

// position for displaying shutter count and other info
#define MENU_DISP_INFO_POS_X 0
#define MENU_DISP_INFO_POS_Y 395

// position for displaying clock outside LV
#define DISPLAY_CLOCK_POS_X 200
#define DISPLAY_CLOCK_POS_Y 410

#define MENU_DISP_ISO_POS_X 500
#define MENU_DISP_ISO_POS_Y 27

// for displaying TRAP FOCUS msg outside LV
#define DISPLAY_TRAP_FOCUS_POS_X 410
#define DISPLAY_TRAP_FOCUS_POS_Y 330
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

//~ #define MENU_NAV_HELP_STRING "Keys: Arrows / SET / PLAY / Q (LiveView) / DISP" 
#define MENU_NAV_HELP_STRING (PLAY_MODE ? "LiveV outside menu: show LV tools          SET/PLAY/LV/DISP" : "SET/PLAY/LV=change values    MENU=Easy/Advanced   DISP=Help")

#define DIALOG_MnCardFormatBegin (0x242AC+4) // ret_CreateDialogBox(...DlgMnCardFormatBegin_handler...) is stored there
#define DIALOG_MnCardFormatExecute (0x24398+4) // similar

#define BULB_MIN_EXPOSURE 100

// http://magiclantern.wikia.com/wiki/Fonts
#define BFNT_CHAR_CODES    0xFF61E250
#define BFNT_BITMAP_OFFSET 0xFF6206D8
#define BFNT_BITMAP_DATA   0xFF622B60

#define DLG_SIGNATURE 0x414944

// from CFn
#define AF_BTN_HALFSHUTTER 0
#define AF_BTN_STAR 1

#define IMGPLAY_ZOOM_LEVEL_ADDR (0x14BB0) // dec GuiImageZoomDown and look for a negative counter
#define IMGPLAY_ZOOM_LEVEL_MAX 14

#define BULB_EXPOSURE_CORRECTION 150 // min value for which bulb exif is OK

#define WINSYS_BMP_DIRTY_BIT_NEG MEM(0x1888c+0x30)
// DebugMsg(4, 2, msg='Whole Screen Backup end')
// winsys_struct.WINSYS_BMP_DIRTY_BIT_NEG /*off_0x30, 0x188BC*/ = 0

#define BTN_ZEBRAS_FOR_PLAYBACK BGMT_Q // what button to use for zebras in Play mode

// manual exposure overrides
#define LVAE_STRUCT 0x168B8
#define CONTROL_BV      (*(uint16_t*)(LVAE_STRUCT+0x16)) // EP_SetControlBv
#define CONTROL_BV_TV   (*(uint16_t*)(LVAE_STRUCT+0x18)) // EP_SetControlParam
#define CONTROL_BV_AV   (*(uint16_t*)(LVAE_STRUCT+0x1a))
#define CONTROL_BV_ISO  (*(uint16_t*)(LVAE_STRUCT+0x1c))
#define CONTROL_BV_ZERO (*(uint16_t*)(LVAE_STRUCT+0x1e))
#define LVAE_ISO_SPEED  (*(uint8_t* )(LVAE_STRUCT))      // offset 0x0; at 3 it changes iso very slowly
#define LVAE_ISO_MIN    (*(uint8_t* )(LVAE_STRUCT+0x24)) // string: ISOMin:%d
#define LVAE_ISO_HIS    (*(uint8_t* )(LVAE_STRUCT+0x26)) // no idea what this is
#define LVAE_DISP_GAIN  (*(uint16_t*)(LVAE_STRUCT+0x20)) // lvae_setdispgain
#define LVAE_MOV_M_CTRL (*(uint8_t* )(LVAE_STRUCT+0x5c)) // lvae_setmoviemanualcontrol

#define MIN_MSLEEP 11

#define INFO_BTN_NAME "DISP"
#define Q_BTN_NAME "LV button"
#define ARROW_MODE_TOGGLE_KEY "LCD sensor"

#define DISPLAY_IS_ON MEM(0x2600) // TurnOnDisplay (PUB) Type=%ld fDisplayTurnOn=%ld

#define LV_STRUCT_PTR 0x1d78
#define FRAME_ISO *(uint16_t*)(MEM(LV_STRUCT_PTR) + 0x56)

// see "Malloc Information"
#define MALLOC_STRUCT 0x24d48
#define MALLOC_FREE_MEMORY (MEM(MALLOC_STRUCT + 24 + 4) - MEM(MALLOC_STRUCT + 24 + 8)) // "Total Size" - "Allocated Size"
