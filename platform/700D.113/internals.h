/**
 * Camera internals for 700D 1.1.3
 */

/** Camera **/
#define CONFIG_MLU
#define CONFIG_ZOOM_BTN_NOT_WORKING_WHILE_RECORDING

/** System **/
#define CONFIG_DIGIC_V
#define CONFIG_STATE_OBJECT_HOOKS
#define CONFIG_PROP_REQUEST_CHANGE //WARNING : CAN CAUSE DAMAGE
#define CONFIG_NO_ADDITIONAL_VERSION
#define CONFIG_EDMAC_MEMCPY
//~#define CONFIG_RESTORE_AFTER_FORMAT  //Freezes at end of format, not stable.
#define CONFIG_BULB
#define CONFIG_MOVIE_AE_WARNING

/** LiveView **/
#define CONFIG_LIVEVIEW

/** Movie mode **/
#define CONFIG_MOVIE
#define CONFIG_NO_DEDICATED_MOVIE_MODE

/** ISO (HDR video, gradual exposure) **/
#define CONFIG_FRAME_ISO_OVERRIDE
#define CONFIG_FRAME_ISO_OVERRIDE_ANALOG_ONLY

/** Display **/
#define CONFIG_3_2_SCREEN
#define CONFIG_VARIANGLE_DISPLAY
#define CONFIG_CAN_REDIRECT_DISPLAY_BUFFER_EASILY
#define CONFIG_CAN_REDIRECT_DISPLAY_BUFFER
#define CONFIG_TOUCHSCREEN
#define CONFIG_EVF_STATE_SYNC
#define CONFIG_DISPLAY_FILTERS
#define CONFIG_PHOTO_MODE_INFO_DISPLAY
#define CONFIG_LCD_SENSOR

/** We can playback sounds via ASIF DMA **/
//#define CONFIG_BEEP //Works, but to many tasks or cam to deal with

/** File I/O **/
#define CONFIG_FIO_RENAMEFILE_WORKS

/** FPS Override **/
#define CONFIG_FPS_UPDATE_FROM_EVF_STATE

/** RAW **/
#define CONFIG_RAW_LIVEVIEW
#define CONFIG_RAW_PHOTO
