/** 
 * Experiments on state objects 
 * 
 * http://magiclantern.wikia.com/wiki/StateObjects
 * 
 **/


#include "dryos.h"
#include "bmp.h"
#include "state-object.h"
#include <platform/state-object.h>
#include "property.h"
#if defined(CONFIG_MODULES)
#include "module.h"
#endif

#ifdef CONFIG_STATE_OBJECT_HOOKS

/*
static void stateobj_matrix_copy_for_patching(struct state_object * stateobj)
{
    int size = stateobj->max_inputs * stateobj->max_states * sizeof(struct state_transition);
    struct state_transition * new_matrix = (struct state_transition *)AllocateMemory(size);
    memcpy(new_matrix, stateobj->state_matrix, size);
    stateobj->state_matrix = new_matrix;
}

static void stateobj_install_hook(struct state_object * stateobj, int input, int state, void* newfunc)
{
    if ((uint32_t)(stateobj->state_matrix) & 0xFF000000) // that's in ROM, make a copy to allow patching
        stateobj_matrix_copy_for_patching(stateobj);
    STATE_FUNC(stateobj,input,state) = newfunc;
}
*/

static void FAST vsync_func() // called once per frame.. in theory :)
{
    #if defined(CONFIG_MODULES)
    module_exec_cbr(CBR_VSYNC);
    #endif

    #if !defined(CONFIG_EVF_STATE_SYNC)
    // for those cameras, it's called from a different spot of the evf state object
    hdr_step();
    #endif

    #if !defined(CONFIG_DIGIC_V) && !defined(CONFIG_7D)
    vignetting_correction_apply_regs();
    #endif

    #ifdef FEATURE_FPS_OVERRIDE
    #ifdef CONFIG_FPS_UPDATE_FROM_EVF_STATE
    fps_update_timers_from_evfstate();
    #endif
    #endif

    digic_iso_step();
    image_effects_step();

    #ifdef FEATURE_DISPLAY_SHAKE
    display_shake_step();
    #endif
}

#ifdef CONFIG_550D
int display_is_on_550D = 0;
int get_display_is_on_550D() { return display_is_on_550D; }
#endif

#ifndef CONFIG_7D_MASTER
int display_is_on() { return DISPLAY_IS_ON; }
#endif

#ifdef FEATURE_SHOW_STATE_FPS
#define num_states 4
#define num_inputs 32
static int state_matrix[num_states][num_inputs];
#endif

static int (*StateTransition)(void*,int,int,int,int) = 0;
static int FAST stateobj_lv_spy(struct state_object * self, int x, int input, int z, int t)
{
    int old_state = self->current_state;

#ifdef FEATURE_SHOW_STATE_FPS
    if (self == DISPLAY_STATE) {
        state_matrix[old_state][input]++;
    }
#endif
#ifdef CONFIG_550D
    if (self == DISPLAY_STATE && old_state != 0 && input == 0) // TurnOffDisplay_action
        display_is_on_550D = 0;
#endif

// sync ML overlay tools (especially Magic Zoom) with LiveView
// this is tricky...
#if defined(CONFIG_5D3) || defined(CONFIG_6D)
    if (self == DISPLAY_STATE && (input == INPUT_ENABLE_IMAGE_PHYSICAL_SCREEN_PARAMETER))
        lv_vsync_signal();

#elif defined(CONFIG_5D2)
    if (self == LV_STATE)//&& old_state == 4)
    {
        //~ lv_vsync_signal();
    }
#elif defined(CONFIG_60D)
    if (self == EVF_STATE && input == 5 && old_state == 5) // evfReadOutDoneInterrupt
        lv_vsync_signal();
#elif defined(CONFIG_600D)
    if (self == EVF_STATE && old_state == 5) {  
		//600D Goes 3 - 4 - 5 5 and 3 ever 1/2 frame
        lv_vsync_signal();
	}
#elif defined(CONFIG_650D) || defined(CONFIG_700D) || defined(CONFIG_100D) //TODO: Check 700D and 100D
    if (self == DISPLAY_STATE && (input == INPUT_SET_IMAGE_VRAM_PARAMETER_MUTE_FLIP_CBR)) {
        lv_vsync_signal();
    }
#elif defined(CONFIG_EOSM)
    if (self == EVF_STATE && input == 15 && old_state == 5) {
        lv_vsync_signal();
    }
#endif

// sync display filters (for these, we need to redirect display buffers
    #ifdef DISPLAY_STATE
    #ifdef CONFIG_CAN_REDIRECT_DISPLAY_BUFFER_EASILY
    if (self == DISPLAY_STATE && input == INPUT_ENABLE_IMAGE_PHYSICAL_SCREEN_PARAMETER)
    {
        #ifdef CONFIG_MODULES
        if (module_exec_cbr(CBR_VSYNC_DISPLAY) == CBR_RET_CONTINUE)
        #endif
        #ifdef FEATURE_HDR_VIDEO
        if (hdr_kill_flicker() == CBR_RET_CONTINUE)
        #endif
        #ifdef CONFIG_DISPLAY_FILTERS
        if (display_filter_lv_vsync(old_state, x, input, z, t) == CBR_RET_CONTINUE)
        #endif
        #ifdef FEATURE_MAGIC_ZOOM
        digic_zoom_overlay_step(0);
        #endif
        0;
    }
    #endif
    #endif
    
#if defined(CONFIG_5D2)
    if (self == LV_STATE && old_state == 2 && input == 2) // lvVdInterrupt
    {
        display_filter_lv_vsync(old_state, x, input, z, t);
    }
#endif

    int ans = StateTransition(self, x, input, z, t);

#ifdef CONFIG_550D
    if (self == DISPLAY_STATE)
        display_is_on_550D = (self->current_state == 1);
#endif


// sync digic functions (like overriding ISO or image effects)

    #if defined(CONFIG_5D2) || defined(CONFIG_50D) || defined(CONFIG_500D)
    if (self == LV_STATE && input==4 && old_state==4) // AJ_ResetPSave_n_WB_n_LVREC_MVR_EV_EXPOSURESTARTED => perfect sync for digic on 5D2 :)
    #elif defined(CONFIG_550D)
    if (self == LV_STATE && input==5 && old_state == 5) // SYNC_GetEngineResource => perfect sync for digic :)
    #elif defined(CONFIG_EVF_STATE_SYNC)
    if (self == EVF_STATE && input == 5 && old_state == 5) // evfReadOutDoneInterrupt => perfect sync for digic :)
    #else
    if (0)
    #endif
    {
        vsync_func();
    }
    
    #if defined(CONFIG_7D_MASTER) || defined(CONFIG_7D)
    if (self == LV_STATE && input==3 && old_state == 3)
        vignetting_correction_apply_lvmgr(x);
    #endif
    
    #if !defined(CONFIG_7D_MASTER) && defined(CONFIG_7D)
    if (self == LV_STATE && input==5 && old_state == 5)
        vsync_func();
    #endif

    #ifdef EVF_STATE
    if (self == EVF_STATE && input == 4 && old_state == 5) // evfSetParamInterrupt
    {
        #if defined(CONFIG_EVF_STATE_SYNC) // exception for overriding ISO
        hdr_step();
        #endif
        
        #ifdef CONFIG_DIGIC_V
        vignetting_correction_apply_regs();
        #endif
    }
    #endif
    return ans;
}

#ifdef CONFIG_5DC
static int stateobj_em_spy(struct state_object * self, int x, int input, int z, int t)
{
    int ans = StateTransition(self, x, input, z, t);

    if (z == 0x0) { fake_simple_button(BGMT_PRESS_HALFSHUTTER); }
    if (z == 0xB) { fake_simple_button(BGMT_UNPRESS_HALFSHUTTER); }
    return ans;
}
#endif

#ifdef SCS_STATE
static int stateobj_scs_spy(struct state_object * self, int x, int input, int z, int t)
{
    int ans = StateTransition(self, x, input, z, t);
    //scs_iso_override_step(); todo
    return ans;
}
#endif

#ifdef SSS_STATE
static int stateobj_sss_spy(struct state_object * self, int x, int input, int z, int t)
{
    int old_state = self->current_state;
    int ans = StateTransition(self, x, input, z, t);
    int new_state = self->current_state;

    #if defined(CONFIG_5D3) || defined(CONFIG_6D)
    if (old_state == 9 && input == 11 && new_state == 9) // sssCompleteMem1ToRaw
        raw_buffer_intercept_from_stateobj();
    #endif

    #if defined(CONFIG_650D) || defined(CONFIG_EOSM) || defined(CONFIG_700D) || defined(CONIFG_100D) //TODO: Check 700D and 100D
    if (old_state == 10 && input == 11 && new_state == 2) // delayCompleteRawtoSraw
        raw_buffer_intercept_from_stateobj();
    #endif

    return ans;
}
#endif

#ifdef SDS_FRONT3_STATE
static int stateobj_sdsf3_spy(struct state_object * self, int x, int input, int z, int t)
{
    int old_state = self->current_state;
    int ans = StateTransition(self, x, input, z, t);
    int new_state = self->current_state;
	
    #if defined(CONFIG_5D2) || defined(CONFIG_550D) || defined(CONFIG_600D) || defined(CONFIG_7D) || defined(CONFIG_1100D)
    // SDSf3:(0)  --  3 sdsMem1toRAWcompress-->(1)
    // SDSf3:(1)  --  3 sdsMem1toJpegDevelop-->(1)
    if (old_state == 0 && input == 3 && new_state == 1)
	{
        raw_buffer_intercept_from_stateobj();
    }   
    #elif defined(CONFIG_50D)
    //~ FrontState - There are only 2
    //~ * FF882F00 - Mem1toJpeg
    //~ * FF882C5C - Mem1 to raw
    //~ * 9 -> Raw (3) -> 10 -> Jpeg(3) -> 8
    if (old_state == 9 && input == 3 && new_state == 10)
    {
        raw_buffer_intercept_from_stateobj();
    }
	#endif

    return ans;
}
#endif

static int stateobj_start_spy(struct state_object * stateobj, void* spy)
{
    if (!StateTransition)
        StateTransition = (void *)stateobj->StateTransition_maybe;
    
    // double check if all states use the same transition function (they do, in theory)
    else if ((void*)StateTransition != (void*)stateobj->StateTransition_maybe)
    {
        #ifndef CONFIG_7D_MASTER
        beep();
        #endif
        return 1;
    }
    
    stateobj->StateTransition_maybe = spy;
    return 0; //not used currently
}

static void state_init(void* unused)
{
    #ifdef DISPLAY_STATE
        stateobj_start_spy(DISPLAY_STATE, stateobj_lv_spy);
    #endif
    #ifdef LV_STATE
        while(!LV_STATE) msleep(50);
        stateobj_start_spy(LV_STATE, stateobj_lv_spy);
    #endif
    #ifdef EVF_STATE
        stateobj_start_spy(EVF_STATE, stateobj_lv_spy);
    #endif
    
    #ifdef EMState
        stateobj_start_spy(EMState, stateobj_em_spy);
    #endif

    #ifdef SCS_STATE
        stateobj_start_spy(SCS_STATE, stateobj_scs_spy);
    #endif
    
    #ifdef SSS_STATE
        stateobj_start_spy(SSS_STATE, stateobj_sss_spy);
    #endif
    
    #ifdef SDS_FRONT3_STATE
        stateobj_start_spy(SDS_FRONT3_STATE, stateobj_sdsf3_spy);
    #endif
    
    #ifdef CONFIG_550D
    display_is_on_550D = (DISPLAY_STATEOBJ->current_state != 0);
    #endif
}

#ifndef CONFIG_QEMU // in QEMU, the state objects are not initialized, so just skip this for now to avoid infinite loops
INIT_FUNC("state_init", state_init);
#endif

#ifdef FEATURE_SHOW_STATE_FPS
void update_state_fps() {
    NotifyBox(1000,"Logging");
    FILE* state_log_file = 0;
    state_log_file = FIO_CreateFileEx(CARD_DRIVE "state.log");
    if(state_log_file) {
        for(int i=0;i<num_states;++i) {
            for(int j=0;j<num_inputs;++j) {
                if(state_matrix[i][j]) {
                    my_fprintf(state_log_file,"%02d %02d %03d\n", i, j, state_matrix[i][j]);
                    state_matrix[i][j] = 0;
                }
            }
        }
        FIO_CloseFile(state_log_file);
        state_log_file = 0;
    }
    NotifyBox(1000,"Done");

}
#endif

#endif
