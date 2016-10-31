/**
 * Blink decoder
 */

#include <module.h>
#include <dryos.h>
#include <config.h>
#include <property.h>
#include <math.h>
#include <bmp.h>
#include <console.h>
#include <zebra.h>

#define LUMA_THRESHOLD 32   /* brighter signals are considered "led on" */

static struct msg_queue * vsync_mq = 0;

/* duration of last 8 pulses */
static int pulses[8] = { 0 };
static int pulses_index = 0;

/* threshold for telling the difference between short pulses (bit=0) and long pulses (bit=1) - autodetected */
static int bit_threshold = 0;

static int get_bit()
{
    static int prev_state = -1;
    static int count = 0;

    while(1)
    {
        int Y;
        int err = msg_queue_receive(vsync_mq, &Y, 1000);
        if (err) continue;
        
        int state = (Y > LUMA_THRESHOLD);
        if (state) info_led_on(); else info_led_off();

        if (prev_state == state)
        {
            count++;
        }
        else
        {
            if (prev_state == 1)    /* on -> off */
            {
                //~ printf("%d ", count);
                pulses[(pulses_index++) & 7] = count;
                int last_bit = (count > bit_threshold) ? 1 : 0;
                count = 1;
                prev_state = state;
                return last_bit;
            }
            count = 1;
            prev_state = state;
        }
    }
}

static char get_byte()
{
    char byte = 0;
    for (int i = 0; i < 8; i++)
    {
        int bit = get_bit();
        if (bit) byte |= (1 << i);
    }
    return byte;
}

static void sync_byte(char expected_value)
{
    printf("Syncing...\n");
    
    char byte = 0;
    while (byte != expected_value)
    {
        byte >>= 1;
        int bit = get_bit();
        if (bit) byte |= 0x80;
        
        /* update threshold */
        int ps[8];
        memcpy(ps, pulses, sizeof(ps));

        printf("Received: %x (", byte);
        for (int i = 0; i < 8; i++)
            printf("%d ", ps[MOD(pulses_index+i, 8)]);
        printf(") => ");

        for (int i = 0; i < 8; i++)
        {
            for (int j = i+1; j < 8; j++)
            {
                if (ps[i] > ps[j])
                {
                    int aux = ps[i];
                    ps[i] = ps[j];
                    ps[j] = aux;
                }
            }
        }

        bit_threshold = (ps[1] + ps[6]) / 2;
        printf("thr=%d\n", bit_threshold);
        
        if (ps[0] <= 1) printf("Warning: blinking too fast\n");
    }
    printf("Sync OK.\n");
}

static int deblink_running = 0;

static void deblink_task()
{
    msleep(1000);
    console_show();
    printf("Point the camera to a blinking LED...\n");
    
    deblink_running = 1;
    
    sync_byte(0xA5);

    while (1)
    {
        int ch = get_byte(0);
        if (ch == 0xA5) continue;
        printf("%s", (char*) &ch);
    }
}

static unsigned int FAST deblink_vsync_cbr(unsigned int unused)
{
    if (!deblink_running)
    {
        return 0;
    }
    
    int Y,U,V;
    get_spot_yuv(10, &Y, &U, &V);
    
    msg_queue_post(vsync_mq, Y);
    
    return 0;
}

static struct menu_entry deblink_menu[] =
{
    {
        .name           = "Decode LED blinks", 
        .select         = run_in_separate_task,
        .priv           = deblink_task,
        .help           = "Decode messages encoded as LED blinks.",
        .depends_on     = DEP_LIVEVIEW,
    },
};

static unsigned int deblink_init()
{
    vsync_mq = msg_queue_create("vsync", 50);
    menu_add("Debug", deblink_menu, COUNT(deblink_menu));
    return 0;
}

static unsigned int deblink_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(deblink_init)
    MODULE_DEINIT(deblink_deinit)
MODULE_INFO_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_VSYNC, deblink_vsync_cbr, 0)
MODULE_CBRS_END()
