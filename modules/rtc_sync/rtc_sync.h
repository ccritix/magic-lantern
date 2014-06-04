
#ifndef __rtc_sync_h__
#define __rtc_sync_h__

#ifdef __rtc_sync_c__
    #define EXT_WEAK_FUNC(f) 
#else
    #define EXT_WEAK_FUNC(f) WEAK_FUNC(f)
#endif


#if defined(MODULE)

int32_t EXT_WEAK_FUNC(ret_0) rtc_sync_get_drift();
uint32_t EXT_WEAK_FUNC(ret_0) rtc_calibrate();
uint64_t EXT_WEAK_FUNC(ret_0) rtc_get_us_clock_value();

#else

static int32_t (*rtc_sync_get_drift) () = MODULE_FUNCTION(rtc_sync_get_drift);
static uint32_t (*rtc_calibrate) () = MODULE_FUNCTION(rtc_calibrate);
static uint64_t (*rtc_get_us_clock_value) () = MODULE_FUNCTION(rtc_get_us_clock_value);

#endif
#endif
