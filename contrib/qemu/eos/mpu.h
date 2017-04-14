#ifndef EOS_MPU_H
#define EOS_MPU_H

#include "eos.h"

void mpu_spells_init(EOSState *s);
void mpu_handle_sio3_interrupt(EOSState *s);
void mpu_handle_mreq_interrupt(EOSState *s);
unsigned int eos_handle_mpu(unsigned int parm, EOSState *s, unsigned int address, unsigned char type, unsigned int value );
unsigned int eos_handle_sio3 ( unsigned int parm, EOSState *s, unsigned int address, unsigned char type, unsigned int value );
unsigned int eos_handle_mreq ( unsigned int parm, EOSState *s, unsigned int address, unsigned char type, unsigned int value );

void mpu_send_keypress(EOSState *s, int keycode);

enum button_codes
{
    BGMT_INFO, BGMT_MENU, BGMT_PLAY,
    BGMT_PRESS_SET, BGMT_UNPRESS_SET, BGMT_JOY_CENTER,
    BGMT_PRESS_DOWN, BGMT_PRESS_DOWN_LEFT, BGMT_PRESS_DOWN_RIGHT, 
    BGMT_PRESS_LEFT, BGMT_PRESS_RIGHT, 
    BGMT_PRESS_UP, BGMT_PRESS_UP_LEFT, BGMT_PRESS_UP_RIGHT,
    BGMT_UNPRESS_DOWN, BGMT_UNPRESS_LEFT, BGMT_UNPRESS_RIGHT, BGMT_UNPRESS_UP,
    BGMT_UNPRESS_UDLR,
    BGMT_PRESS_ZOOM_IN, BGMT_PRESS_ZOOM_OUT,
    BGMT_UNPRESS_ZOOM_IN, BGMT_UNPRESS_ZOOM_OUT,
    BGMT_Q, BGMT_Q_ALT, BGMT_LV, BGMT_PRESS_AV, BGMT_UNPRESS_AV,
    BGMT_JUMP, BGMT_PRESS_DIRECT_PRINT,
    BGMT_TRASH, BGMT_UNLOCK, BGMT_PICSTYLE, BGMT_LIGHT, 
    BGMT_ISO, BGMT_AFPAT_UNPRESS, BGMT_PRESS_DISP, BGMT_UNPRESS_DISP,
    BGMT_PRESS_RAW_JPEG, BGMT_UNPRESS_RAW_JPEG,
    BGMT_WHEEL_DOWN, BGMT_WHEEL_LEFT, BGMT_WHEEL_RIGHT, BGMT_WHEEL_UP,
    GMT_GUICMD_OPEN_SLOT_COVER, GMT_GUICMD_START_AS_CHECK,
    GMT_GUICMD_OPEN_BATT_COVER,
    GMT_GUICMD_PRESS_BUTTON_SOMETHING,
    GMT_LOCAL_DIALOG_REFRESH_LV,
    BGMT_END_OF_LIST
};

#endif

