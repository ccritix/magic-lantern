#include "all_features.h"
/* Can't Be Implemented */
#undef FEATURE_FORCE_LIVEVIEW // Already Live View
#undef FEATURE_MLU // No Mirror
#undef FEATURE_MLU_HANDHELD
#undef FEATURE_STICKY_DOF // No DOF button
#undef FEATURE_IMAGE_EFFECTS // DigicV new effects check "Art Filter"
#undef CONFIG_MEMPATCH_CHECK // Reports 0 for total, like 6D
#undef FEATURE_INTERMEDIATE_ISO_PHOTO_DISPLAY // Well.. it will work in 1 mode
#undef FEATURE_AF_PATTERNS // No regular AF
#undef FEATURE_VOICE_TAGS // Just to be sure
#undef FEATURE_PLAY_422 // No way to select
#undef FEATURE_SHUTTER_FINE_TUNING //Doesn't seem to work


/* Working */
#define FEATURE_CROP_MODE_HACK
#define FEATURE_PREFIX //Named HDR Bracketing B01, B02, etc
#define FEATURE_NOHELP //No one can help you now
#define FEATURE_VIDEO_HACKS
#define FEATURE_TIMER_HACK


/* Debug */
#undef CONFIG_STRESS_TEST // We don't need this so much.
//~ #undef SHOULD_CHECK_SIG
#undef CONFIG_AUTOBACKUP_ROM //WTF, Batman?
#define CONFIG_HEXDUMP
#define CONFIG_TSKMON
#define CONFIG_DEBUG_INTERCEPT
#define FEATURE_PROP_DISPLAY
//~ #define CONFIG_DIGIC_POKE
//~ #define FEATURE_ISR_HOOKS
//~ #define FEATURE_SHOW_SIGNATURE



/* Some Hope Yet */
//~ #undef FEATURE_AUDIO_REMOTE_SHOT
#undef FEATURE_TRAP_FOCUS
#undef FEATURE_FOLLOW_FOCUS
#undef FEATURE_RACK_FOCUS
#undef FEATURE_FOCUS_STACKING
#undef FEATURE_GHOST_IMAGE // No way to pick image but works.
#undef FEATURE_SET_MAINDIAL // Set taken over by Q
#undef FEATURE_PLAY_EXPOSURE_FUSION
#undef FEATURE_PLAY_COMPARE_IMAGES
#undef FEATURE_PLAY_TIMELAPSE
#undef FEATURE_PLAY_EXPOSURE_ADJUST
#undef FEATURE_QUICK_ZOOM
#undef FEATURE_QUICK_ERASE
#undef FEATURE_LV_FOCUS_BOX_FAST
#undef FEATURE_LV_FOCUS_BOX_SNAP
#undef FEATURE_ARROW_SHORTCUTS


//~ #define FEATURE_LV_FOCUS_BOX_SNAP_TO_X5_RAW
//~ #define FEATURE_LV_FOCUS_BOX_AUTOHIDE

#undef FEATURE_OVERLAYS_IN_PLAYBACK_MODE
//~ #undef FEATURE_AUDIO_METERS // Work While Recording
//~ #undef FEATURE_LV_FOCUS_BOX_AUTOHIDE // Works in FB mode

#define FEATURE_FOCUS_PEAK_DISP_FILTER //Need early sync.

//~ #define FEATURE_PICQ_DANGEROUS // All 3 Props are len 4. Show up as len0? Mraw/Sraw causes crash.

#define FEATURE_HEADPHONE_MONITORING
#define CONFIG_DUMP_AUDIO

#define FEATURE_EYEFI_TRICKS
