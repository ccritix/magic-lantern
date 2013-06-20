#include "all_features.h"

#undef FEATURE_IMAGE_REVIEW_PLAY // not needed, one can press Zoom right away
#undef FEATURE_QUICK_ZOOM // Canon has it
#undef FEATURE_QUICK_ERASE // Canon has it
#undef FEATURE_IMAGE_EFFECTS // none working
#undef FEATURE_MOVIE_REC_KEY // Canon has it
#undef FEATURE_NITRATE_WAV_RECORD // not implemented
#undef FEATURE_AF_PATTERNS // Canon has it
#undef FEATURE_MLU_HANDHELD // not needed, Canon's silent mode is much better
#undef FEATURE_SHUTTER_LOCK // Canon has a dedicated button for it
#undef FEATURE_FLASH_TWEAKS // no built-in flash

//~ #define FEATURE_ISR_HOOKS // doesn't compile
#define FEATURE_KEN_ROCKWELL_ZOOM_5D3
#define FEATURE_ZOOM_TRICK_5D3 // not reliable
//~ #define FEATURE_REMEMBER_LAST_ZOOM_POS_5D3 // too many conflicts with other features
#undef FEATURE_IMAGE_POSITION

//~ #define FEATURE_ISR_HOOKS

#undef FEATURE_LV_ZOOM_AUTO_EXPOSURE // seems to cause black pictures

//~ #define FEATURE_VIDEO_HACKS

#define FEATURE_AFMA_TUNING

#undef FEATURE_VOICE_TAGS // no sound recorded

#define FEATURE_JOY_CENTER_ACTIONS 

#define FEATURE_LV_FOCUS_BOX_SNAP_TO_X5_RAW
