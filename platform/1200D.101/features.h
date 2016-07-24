#include "all_features.h"

// No MLU on the 1200D
#undef FEATURE_MLU
#undef FEATURE_MLU_HANDHELD

// Disable almost all audio stuff
#undef FEATURE_WAV_RECORDING
#undef FEATURE_FPS_WAV_RECORD
#undef FEATURE_BEEP
#undef FEATURE_VOICE_TAGS
#undef FEATURE_AUDIO_REMOTE_SHOT

#undef FEATURE_ARROW_SHORTCUTS
