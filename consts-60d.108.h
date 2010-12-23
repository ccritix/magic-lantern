#ifndef CONSTS_60D_H
#define CONSTS_60D_H 1

// hack.c
#define HIJACK_INSTR_BL_CSTART  0xFF01019C 
#define HIJACK_INSTR_BSS_END 0xFF0110D0
#define HIJACK_FIXBR_BZERO32 0xFF011038
#define HIJACK_FIXBR_CREATE_ITASK 0xFF0110C0
#define HIJACK_INSTR_MY_ITASK 0xFF0110DC
#define HIJACK_TASK_ADDR 0x1a2c

#define ROMBASEADDR	0xFF010000
#define RESTARTSTART	0x0008B000

// gui.c
#define gui_main_task_functable 0xFF53DA40

// debug.c

// zebra.c
// find "[DISP] SetImageVramParameter pAddress=" on dmlog
#define yuv422_image_buffer 0x41b07800

/*
550d 109
#define FOCUS_CONFIRMATION 0x41d0
#define DISPLAY_SENSOR 0x2dec
#define DISPLAY_SENSOR_MAYBE 0xC0220104
*/


#endif CONSTS_60D_H
