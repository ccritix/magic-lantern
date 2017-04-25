#include <stddef.h>
#include "model_list.h"

struct eos_model_desc eos_model_list[] = {
    {
        /* defaults for DIGIC 3 cameras */
        .digic_version          = 3,
        .rom1_addr              = 0xF8000000,
        .rom1_size              = 0x00800000,
        .ram_size               = 0x10000000,
        .caching_bit            = 0x10000000,
        .atcm_addr              = 0x00000000,
        .atcm_size              = 0x00001000,
        .btcm_addr              = 0x40000000,
        .btcm_size              = 0x00001000,
        .io_mem_size            = 0x10000000,
        .firmware_start         = 0xFF810000,
        .bootflags_addr         = 0xF8000000,
        .current_task_name_offs = 0x0D,
        .hptimer_interrupt      = 0x10,
        .sd_driver_interrupt    = 0x4A,
        .sd_dma_interrupt       = 0x29,
        .card_led_address       = 0xC02200E0,
        .mpu_request_register   = 0xC0220098,
        .mpu_status_register    = 0xC0220098,
        .mpu_control_register   = 0xC0203034,
        .mpu_request_interrupt  = 0x52,
    },
    {
        /* defaults for DIGIC 4 cameras */
        .digic_version          = 4,
        /* note: some cameras have smaller ROMs, or only one ROM */
        .rom0_addr              = 0xF0000000,
        .rom0_size              = 0x01000000,
        .rom1_addr              = 0xF8000000,
        .rom1_size              = 0x01000000,
        .ram_size               = 0x20000000,
        .caching_bit            = 0x40000000,
        .atcm_addr              = 0x00000000,
        .atcm_size              = 0x00001000,
        .btcm_addr              = 0x40000000,
        .btcm_size              = 0x00001000,
        .io_mem_size            = 0x10000000,
        .firmware_start         = 0xFF010000,
        .bootflags_addr         = 0xF8000000,
        .current_task_name_offs = 0x09,
        .dryos_timer_id         = 2,
        .dryos_timer_interrupt  = 0x0A,
        .hptimer_interrupt      = 0x10,
        .sd_driver_interrupt    = 0xB1,
        .sd_dma_interrupt       = 0xB8,
        .card_led_address       = 0xC0220134,   /* SD */
        .mpu_request_register   = 0xC022009C,
        .mpu_status_register    = 0xC022009C,
        .mpu_control_register   = 0xC020302C,
        .mpu_request_interrupt  = 0x50,
    },
    {
        /* defaults for DIGIC 5 cameras */
        .digic_version          = 5,
        .rom0_addr              = 0xF0000000,
        .rom0_size              = 0x01000000,
        .rom1_addr              = 0xF8000000,
        .rom1_size              = 0x01000000,
        .ram_size               = 0x20000000,
        .caching_bit            = 0x40000000,
        .atcm_addr              = 0x00000000,
        .atcm_size              = 0x00001000,
        .btcm_addr              = 0x40000000,
        .btcm_size              = 0x00001000,
        .io_mem_size            = 0x20000000,
        .firmware_start         = 0xFF0C0000,
        .bootflags_addr         = 0xF8000000,
        .current_task_name_offs = 0x09,
        .dryos_timer_id         = 2,
        .dryos_timer_interrupt  = 0x0A,
        .hptimer_interrupt      = 0x10,
        .sd_driver_interrupt    = 0x172,
        .sd_dma_interrupt       = 0x171,
        .card_led_address       = 0xC022C188,   /* SD */
        .mpu_control_register   = 0xC020302C,
        .mpu_request_interrupt  = 0x50,
    },
    {
        /* defaults for DIGIC 6 cameras */
        .digic_version          = 6,
        .rom0_size              = 0,    /* not dumped yet, camera locks up (?!) */
        .rom1_addr              = 0xFC000000,
        .rom1_size              = 0x02000000,
        .ram_size               = 0x20000000,
        .caching_bit            = 0x40000000,
        .atcm_addr              = 0x00000000,
        .atcm_size              = 0x00004000,
        .btcm_addr              = 0x80000000,
        .btcm_size              = 0x00010000,
        .ram_extra_addr         = 0xBFE00000,
        .ram_extra_size         = 0x00200000,
        .io_mem_size            = 0x20000000,
        .firmware_start         = 0xFE0A0000,
        .bootflags_addr         = 0xFC040000,
        .current_task_name_offs = 0x09,
        .dryos_timer_id         = 1,
        .dryos_timer_interrupt  = 0x1B,
        .hptimer_interrupt      = 0x28,
        .sd_driver_interrupt    = 0xEE,
        .sd_dma_interrupt       = 0xBE,
        .card_led_address       = 0xD20B0A24,
    },
    {
        /* defaults for DIGIC 7 cameras */
        .digic_version          = 7,
        .current_task_name_offs = 0x09,
        .dryos_timer_id         = 1,
        .dryos_timer_interrupt  = 0x1B,
        .hptimer_interrupt      = 0x28,
    },
    {
        .name                   = "50D",
        .digic_version          = 4,
        .card_led_address       = 0xC02200BC,
    },
    {
        .name                   = "60D",
        .digic_version          = 4,
        .current_task_addr      = 0x1A2C,
    },
    {
        .name                   = "600D",
        .digic_version          = 4,
        .current_task_addr      = 0x1A2C,
    },
    {
        .name                   = "500D",
        .digic_version          = 4,
        .current_task_addr      = 0x1A74,
    },
    {
        .name                   = "5D2",
        .digic_version          = 4,
        .firmware_start         = 0xFF810000,
        .card_led_address       = 0xC02200BC,
    },
    {
        .name                   = "5D3",
        .digic_version          = 5,
        .current_task_addr      = 0x23E14,
        .mpu_request_register   = 0xC02200BC,
        .mpu_status_register    = 0xC02200BC,
        .card_led_address       = 0xC022C06C,
        .cf_driver_interrupt    = 0x82,
        .cf_dma_interrupt       = 0xE3,
    },
    {
        /* started on request on photo taking, raw develop and others;
         * see EekoBltDmac, Eeko WakeUp; runs Thumb-2 code */
        .name                   = "5D3eeko",
        .digic_version          = 50,           /* hack to get an empty configuration */
        .ram_size               = 0x00100000,   /* unknown, mapped to 0xD0288000 on main CPU*/
        .ram_extra_addr         = 0x01E00000,   /* mapped to the same address on main CPU */
        .ram_extra_size         = 0x00200000,   /* 1E0-1F0, 1F0-1F2, 1F2-1F4 (I/O; fixme) */
        .caching_bit            = 0x40000000,   /* D0284000-D0288000: identical to D028C000-D0290000 */
        .io_mem_size            = 0x40000000,   /* really? */
        .atcm_addr              = 0x00000000,   /* not sure, shouldn't do any harm */
        .atcm_size              = 0x00004000,   /* guess: D0288000 ... D028C000 */
        .btcm_addr              = 0x40000000,   /* not sure, appears used, but no memory region configured */
        .btcm_size              = 0x00004000,   /* dump from D0280000 identical to 0xD0288000 after 0x4000 */
        .dryos_timer_id         = 11,           /* see eos_handle_timers for mapping */
        .dryos_timer_interrupt  = 0xFE,
    },
    {
        .name                   = "650D",
        .digic_version          = 5,
    },
    {
        .name                   = "100D",
        .digic_version          = 5,
        .mpu_request_register   = 0xC022006C,
        .mpu_status_register    = 0xC022006C,
        .serial_flash_size      = 0x1000000,
        .current_task_addr      = 0x652AC,
    },
    {
        .name                   = "7D",
        .digic_version          = 4,
        .card_led_address       = 0xC022D06C,
    },
    {
        .name                   = "550D",
        .digic_version          = 4,
        .current_task_addr      = 0x1A20,
    },
    {
        .name                   = "6D",
        .digic_version          = 5,
        .card_led_address       = 0xC022C184,
    },
    {
        .name                   = "70D",
        .digic_version          = 5,
        .current_task_addr      = 0x7AAC0,
        .mpu_request_register   = 0xC02200BC,
        .mpu_status_register    = 0xC02200BC,
        .card_led_address       = 0xC022C06C,
        .serial_flash_size      = 0x800000,
    },
    {
        .name                   = "700D",
        .digic_version          = 5,
        .current_task_addr      = 0x233DC,
        .mpu_request_register   = 0xC022006C,
        .mpu_status_register    = 0xC022006C,
        .card_led_address       = 0xC022C188,
        .serial_flash_size      = 0x800000,
    },
    {
        .name                   = "1100D",
        .digic_version          = 4,
        .current_task_addr      = 0x1A2C,
    },
    {
        .name                   = "1200D",
        .digic_version          = 4,
        .firmware_start         = 0xFF0C0000,
        .current_task_addr      = 0x1A2C,
        .card_led_address       = 0xC0220134,
    },
    {
        .name                   = "1300D",
        .digic_version          = 4,
        .rom0_size              = 0x02000000,
        .rom1_size              = 0x02000000,
        .firmware_start         = 0xFF0C0000,
        .mpu_request_register   = 0xC022D0C4,
        .mpu_status_register    = 0xC022F484,
    },
    {
        .name                   = "EOSM",
        .digic_version          = 5,
        .current_task_addr      = 0x3DE78,
        .mpu_request_register   = 0xC022006C,
        .mpu_status_register    = 0xC022006C,
        .card_led_address       = 0xC022C188,
        .serial_flash_size      = 0x800000,
    },
    {
        .name                   = "EOSM3",
        .digic_version          = 6,
        .firmware_start         = 0xFC000000,
        .rom0_addr              = 0xFB800000,
        .rom0_size              = 0x800000,
        .current_task_addr      = 0x803C,
        .card_led_address       = 0xD20B0994,
    },
    {
        .name                   = "EOSM10",
        .digic_version          = 6,
        .firmware_start         = 0xFC000000,
        .rom0_addr              = 0xFB800000,
        .rom0_size              = 0x800000,
        .current_task_addr      = 0x803C,
        .card_led_address       = 0xD20B0994,   /* unknown, copied from M3 */
    },
    {
        .name                   = "EOSM5",
        .digic_version          = 7,
        .firmware_start         = 0xE0000000,
        .rom1_addr              = 0xE0000000,
        .rom1_size              = 0x02000000,
        .ram_size               = 0x40000000,
        .caching_bit            = 0x40000000,
        .io_mem_size            = 0x1F000000,
        .ram_extra_addr         = 0xDF000000,
        .ram_extra_size         = 0x01000000,
        .current_task_addr      = 0x1020,
    },
    {
        .name                   = "7D2M",
        .digic_version          = 6,
        .current_task_addr      = 0x28568,
        .card_led_address       = 0xD20B0C34,
        .ram_manufacturer_id    = 0x18000103,   /* 80D bootloader */
    },
    {
        .name                   = "7D2S",
        .digic_version          = 6,
        .card_led_address       = 0xD20B0C34,   /* not sure */
        .ram_manufacturer_id    = 0x18000103,   /* 80D bootloader */
    },
    {
        .name                   = "80D",
        .digic_version          = 6,
        .ram_size               = 0x40000000,
        .ram_manufacturer_id    = 0x18000103,   /* RAM manufacturer: Micron */
    },
    {
        .name                   = "750D",
        .digic_version          = 6,
        .ram_manufacturer_id    = 0x14000203,
    },
    {
        .name                   = "760D",
        .digic_version          = 6,
        .ram_manufacturer_id    = 0x14000203,
    },
    {
        .name                   = "5D4",
        .digic_version          = 6,
        .ram_size               = 0x40000000,
        .ram_manufacturer_id    = 0x18000401,
        .card_led_address       = 0xD20B0224,
    },
    {
        .name                   = "5D4AE",
        .digic_version          = 6,
        .ram_manufacturer_id    = 0x18000401,
        .card_led_address       = 0xD20B0224,
    },
    {
        .name                   = "1000D",
        .digic_version          = 3,
        .current_task_addr      = 0x352C0,
    },
    {
        .name                   = "400D",
        .digic_version          = 3,
        .card_led_address       = 0xC0220000,
        .current_task_addr      = 0x27C20,
    },
    {
        .name                   = "450D",
        .digic_version          = 3,
        .current_task_addr      = 0x355C0,
        .sd_driver_interrupt    = 0x4B,
        .sd_dma_interrupt       = 0x32,
    },
    {
        .name                   = "40D",
        .digic_version          = 3,
        .current_task_addr      = 0x22E00,
    },
    {
        .name                   = "5D",
        .digic_version          = 3,            /* actually 2 */
        .io_mem_size            = 0x20000000,
        .card_led_address       = 0xC02200A0,
     /* .current_task_addr      = 0x2D2C4  */   /* fixme: it's MEM(0x2D2C4) */
    },
    {
        .name                   = "A1100",
        .digic_version          = 4,
        .rom0_size              = 0x400000,     /* fixme: unknown */
        .rom1_size              = 0x400000,
        .ram_size               = 0x10000000,   /* fixme: only 64M */
        .btcm_addr              = 0x80000000,
        .io_mem_size            = 0x01000000,
        .card_led_address       = 0xC02200CC,
    },
    {
        .name = NULL,
        .digic_version = 0,
    }
};

