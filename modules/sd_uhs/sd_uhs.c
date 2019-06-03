/**
 * Experimental SD UHS overclocking.
 */

#include <module.h>
#include <dryos.h>
#include <patch.h>
#include <console.h>

static uint32_t sd_enable_18V = 0;
static uint32_t sd_setup_mode = 0;
static uint32_t sd_setup_mode_in = 0;
static uint32_t sd_setup_mode_reg = 0xFFFFFFFF;
static uint32_t sd_set_function = 0;
static uint32_t uhs_regs[]     = { 0xC0400600, 0xC0400604,/*C0400608, C040060C*/0xC0400610, 0xC0400614, 0xC0400618, 0xC0400624, 0xC0400628, 0xC040061C, 0xC0400620 };   /* register addresses */
static uint32_t sdr_160MHz[]   = {        0x2,        0x3,                             0x1, 0x1D000001,        0x0,      0x100,      0x100,      0x100,        0x1 };   /* overclocked values: 160MHz = 96*(4+1)/(2?+1) (found by brute-forcing) */
static uint32_t uhs_vals[COUNT(uhs_regs)];  /* current values */
static int sd_setup_mode_enable = 0;

/* start of the function */
static void sd_setup_mode_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    qprintf("sd_setup_mode(dev=%x)\n", regs[0]);

    /* this function is also used for other interfaces, such as serial flash */
    /* only enable overriding when called with dev=1 */
    sd_setup_mode_enable = (regs[0] == 1);
}

/* called right before the case switch in sd_setup_mode (not at the start of the function!) */
static void sd_setup_mode_in_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    qprintf("sd_setup_mode switch(mode=%x) en=%d\n", regs[sd_setup_mode_reg], sd_setup_mode_enable);

    if (sd_setup_mode_enable && regs[sd_setup_mode_reg] == 4)   /* SDR50? */
    {
        /* set our register overrides */
        for (int i = 0; i < COUNT(uhs_regs); i++)
        {
            MEM(uhs_regs[i]) = uhs_vals[i];
        }

        /* set some invalid mode to bypass the case switch
         * and keep our register values only */
        regs[sd_setup_mode_reg] = 0x13;
    }
}

static void sd_set_function_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    qprintf("sd_set_function(0x%x)\n", regs[0]);

    /* UHS-I SDR50? */
  //  if (regs[0] == 0xff0002)
  //  {
        /* force UHS-I SDR104 */
        regs[0] = 0xff0003;
  //  }
}
static int patch_act = 0;

static void sd_uhs_patch()
{
    if (sd_enable_18V)
    {
        patch_instruction(sd_enable_18V, 0xe3a00000, 0xe3a00001, "SD 1.8V");
    }
    patch_hook_function(sd_setup_mode, MEM(sd_setup_mode), sd_setup_mode_log, "SD UHS");
    patch_hook_function(sd_setup_mode_in, MEM(sd_setup_mode_in), sd_setup_mode_in_log, "SD UHS");

    /* enable SDR104 */
    patch_hook_function(sd_set_function, MEM(sd_set_function), sd_set_function_log, "SDR104");

    /* power-cycle and reconfigure the SD card */
    memcpy(uhs_vals, sdr_160MHz, sizeof(uhs_vals));

    patch_act = 1;
}

static unsigned int sd_uhs_init()
{
    if ((!is_camera("EOSM", "2.0.2")) && (!is_camera("700D", "1.1.5")) && (!is_camera("100D", "1.0.1")) && (!is_camera("6D", "1.1.6")) && (!is_camera("70D", "1.1.2") && (!is_camera("650D", "1.0.4"))))
    {
    	NotifyBox(2000, "sd_uhs.mo is not supported for your camera");
    }

    if (is_camera("EOSM", "2.0.2"))
    {
        sd_setup_mode       = 0xFF338D40;
        sd_setup_mode_in    = 0xFF338DC8;
        sd_setup_mode_reg   = 1;
        sd_set_function     = 0xFF63EF60;
    	sd_uhs_patch();
    }

    if (is_camera("100D", "1.0.1"))
    {
        sd_setup_mode       = 0xFF3355B0;
        sd_setup_mode_in    = 0xFF335648;
        sd_setup_mode_reg   = 1;
        sd_set_function     = 0xFF6530A4;
        sd_uhs_patch();  
    }

    if (is_camera("700D", "1.1.5"))
    {
        sd_setup_mode       = 0xFF3376E8;   /* start of the function */
        sd_setup_mode_in    = 0xFF337770;   /* right before the switch */
        sd_setup_mode_reg   = 1;            /* switch variable is in R1 (likely all D5 other than 5D3) */
        sd_set_function     = 0xFF748F18;
        sd_uhs_patch();     }

    if (is_camera("6D", "1.1.6"))
    {
        sd_setup_mode       = 0xFF325A20;
        sd_setup_mode_in    = 0xFF325AA8;
        sd_setup_mode_reg   = 1;            /* switch variable is in R1 (likely all D5 other than 5D3) */
        sd_set_function     = 0xFF78F308;
        sd_uhs_patch();     }

    if (is_camera("70D", "1.1.2"))
    {
        sd_setup_mode       = 0xFF33E078;
        sd_setup_mode_in    = 0xFF33E100;
        sd_setup_mode_reg   = 1;
        sd_set_function     = 0xFF7CE4B8;
        sd_uhs_patch();     }

    if (is_camera("650D", "1.0.4"))
    {
        sd_setup_mode       = 0xFF334C4C;
        sd_setup_mode_in    = 0xFF334CD4;
        sd_setup_mode_reg   = 1;
        sd_set_function     = 0xFF73FD20;
        sd_uhs_patch();     }

/* Below cams not tested/supported atm. Try it by enabling sd_uhs_patch(); */
    if (is_camera("5D3", "1.1.3"))
    {
        /* sd_setup_mode:
         * sdSendCommand: CMD%d  Retry=... -> 
         * sd_configure_device(1) (called after a function without args) ->
         * sd_setup_mode(dev) if dev is 1 or 2 ->
         * logging hook is placed in the middle of sd_setup_mode_in (not at start)
         */
        sd_setup_mode       = 0xFF47B4C0;   /* start of the function; not strictly needed on 5D3 */
        sd_setup_mode_in    = 0xFF47B4EC;   /* after loading sd_mode in R0, before the switch */
        sd_setup_mode_reg   = 0;            /* switch variable is in R0 */
        sd_set_function     = 0xFF6ADE34;   /* sdSetFunction */
        sd_enable_18V       = 0xFF47B4B8;   /* 5D3 only (Set 1.8V Signaling) */
     /* sd_uhs_patch(); */    }

    if (is_camera("5D3", "1.2.3"))
    {
        sd_setup_mode       = 0xFF484474;
        sd_setup_mode_in    = 0xFF4844A0;
        sd_setup_mode_reg   = 0;
        sd_set_function     = 0xFF6B8FD0;
        sd_enable_18V       = 0xFF48446C;   /* 5D3 only (Set 1.8V Signaling) */
     /* sd_uhs_patch(); */    }

	return 0;
}

static unsigned int sd_uhs_deinit()
{
    return 0;
}


MODULE_INFO_START()
    MODULE_INIT(sd_uhs_init)
    MODULE_DEINIT(sd_uhs_deinit)
MODULE_INFO_END()

