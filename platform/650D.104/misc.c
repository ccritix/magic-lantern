#include "dryos.h"
#include "bmp.h"
#include "config.h"
#include "property.h"
#include "menu.h"
#include "gui.h"
#include "audio-common.c"
#include "boot-hack.h"

void audio_ic_write(uint32_t cmd)
{
    _audio_ic_write(cmd);
}

uint8_t audio_ic_read(uint32_t cmd)
{
    unsigned int value = 0;
    _audio_ic_read(cmd, &value);
    return value;
}

int audio_meters_are_drawn()
{
    return audio_meters_are_drawn_common();
}

void audio_configure(int unused) {}
