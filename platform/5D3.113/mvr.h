#ifndef __PLATFORM_MVR_H__
#define __PLATFORM_MVR_H__

// Movie recording.

extern struct mvr_struct * mvr_struct;
extern struct state_object * mvr_state;

// tab size: 4
// not all values are correct (most of them are for 550D)
struct mvr_config
{
	uint16_t		off_0x0;
	uint16_t		debug_flag;  // 0x02, mvrSetPrintMovieLog
	uint16_t		qscale_mode; // 0x04, mvrFixQScale
	uint16_t		db_filter_a; // 0x06, mvrSetDefDBFilter
	uint16_t		db_filter_b; // 0x08
	int16_t			def_q_scale; // 0x0a, mvrSetDefQScale
	int16_t 		off_0xc;
	int16_t 		off_0xe;
	int16_t 		off_0x10;
	int16_t 		qscale_limit_L; // 0x12, mvrSetLimitQScale
	int16_t 		qscale_limit_H; // 0x14
	uint16_t		time_const;     // 0x16, mvrSetTimeConst
	uint32_t		fullhd_30fps_opt_size_I;// 0x18, mvrSetFullHDOptSize
	uint32_t		fullhd_30fps_opt_size_P;// 0x1c
	uint32_t		D1_30fps;				// 0x20 OK
	uint32_t		D2_30fps;				// 0x24
	uint32_t		x67e4;					// 0x28
	uint32_t		fullhd_25fps_opt_size_I;// 0x2c OK
	uint32_t		fullhd_25fps_opt_size_P;// 0x30
	uint32_t		fullhd_25fps_D1;		// 0x34
	uint32_t		fullhd_25fps_D2;		// 0x38
	uint32_t		x67f8;					// 0x3c
	uint32_t		fullhd_24fps_opt_size_I;// 0x40 OK
	uint32_t		fullhd_24fps_opt_size_P;// 0x44
	uint32_t		fullhd_24fps_D1;		// 0x48
	uint32_t		fullhd_24fps_D2;		// 0x4c
	uint32_t		x680c;					// 0x50
	uint32_t		hd_60fps_opt_size_I;	// 0x54 OK
	uint32_t		hd_60fps_opt_size_P;	// 0x58
	uint32_t		hd_60fps_D1;			// 0x5c
	uint32_t		hd_60fps_D2;			// 0x60
	uint32_t		x6820;					// 0x64
	uint32_t		hd_50fps_opt_size_I;	// 0x68 OK
	uint32_t		hd_50fps_opt_size_P;	// 0x6c
	uint32_t		hd_50fps_D1;			// 0x70
	uint32_t		hd_50fps_D2;			// 0x74
	uint32_t		x6834_kinda_counter;	// 0x78
	uint32_t		vga_60fps_opt_size_I;	// 0x7c hidden mode?
	uint32_t		vga_60fps_opt_size_P;	// 0x80
	uint32_t		vga_60fps_D1;			// 0x84
	uint32_t		vga_60fps_D2;			// 0x88
	uint32_t		x6848;					// 0x8c
	uint32_t		vga_50fps_opt_size_I;	// 0x90
	uint32_t		vga_50fps_opt_size_P;	// 0x94
	uint32_t		vga_50fps_D1;			// 0x98
	uint32_t		vga_50fps_D2;			// 0x9c
	uint32_t		x685c;					// 0xa0
	int32_t 		xa4;					// 0xa4
	int32_t 		xa8;					// 0xa8
	int32_t 		xac;					// 0xac
	uint32_t		xb0;					// 0xb0
	uint32_t		xb4;					// 0xb4
	uint32_t		xb8;					// 0xb8
	uint32_t		xbc;					// 0xbc
	uint32_t		xc0;					// 0xc0
	uint32_t		xc4;					// 0xc4
	uint32_t		xc8;					// 0xc8
	uint32_t		xcc;					// 0xcc
	uint32_t		xd0;					// 0xd0
	uint32_t		xd4;					// 0xd4
	uint32_t		xd8;					// 0xd8
	uint32_t		xdc;					// 0xdc
	uint32_t		xe0;					// 0xe0
	uint32_t		xe4;					// 0xe4
	uint32_t		xe8;					// 0xe8
	uint32_t		xec;					// 0xec
	uint32_t		xf0;					// 0xf0
	int32_t			another_def_q_scale;	// 0xf4
	int32_t			IniQScale;				// 0xf8
	int32_t			actual_qscale_maybe;	// 0xfc
	uint32_t		x100;	// 0x100
	uint32_t		x104;	// 0x104
	uint32_t		x108;	// 0x108
	uint32_t		x10c;	// 0x10c
	uint32_t		x110;	// 0x110
	uint32_t		x114;			// 0x114
	uint32_t		x118;			// 0x118
	uint32_t		x11c;			// 0x11c
	uint32_t		x120;			// 0x120
	uint32_t		x124;			// 0x124
	uint32_t		fullhd_30fps_gop_opt_0;	// 0x128 OK
	uint32_t		fullhd_30fps_gop_opt_1;	// 0x12c
	uint32_t		fullhd_30fps_gop_opt_2;	// 0x130
	uint32_t		fullhd_30fps_gop_opt_3;	// 0x134
	uint32_t		fullhd_30fps_gop_opt_4;	// 0x138
	uint32_t		fullhd_25fps_gop_opt_0;	// 0x13c OK
	uint32_t		fullhd_25fps_gop_opt_1;	// 0x140
	uint32_t		fullhd_25fps_gop_opt_2;	// 0x144
	uint32_t		fullhd_25fps_gop_opt_3;	// 0x148
	uint32_t		fullhd_25fps_gop_opt_4;	// 0x14c
	uint32_t		fullhd_24fps_gop_opt_0;	// 0x150 OK
	uint32_t		fullhd_24fps_gop_opt_1;	// 0x154
	uint32_t		fullhd_24fps_gop_opt_2;	// 0x158
	uint32_t		fullhd_24fps_gop_opt_3;	// 0x15c
	uint32_t		fullhd_24fps_gop_opt_4;	// 0x160
	uint32_t		hd_60fps_gop_opt_0;		// 0x164 OK
	uint32_t		hd_60fps_gop_opt_1;		// 0x168
	uint32_t		hd_60fps_gop_opt_2;		// 0x16c
	uint32_t		hd_60fps_gop_opt_3;		// 0x170
	uint32_t		hd_60fps_gop_opt_4;		// 0x174
	uint32_t		hd_50fps_gop_opt_0;		// 0x178 OK
	uint32_t		hd_50fps_gop_opt_1;		// 0x17c
	uint32_t		hd_50fps_gop_opt_2;		// 0x180
	uint32_t		hd_50fps_gop_opt_3;		// 0x184
	uint32_t		hd_50fps_gop_opt_4;		// 0x188
	uint32_t		vga_60fps_gop_opt_0;	// 0x18c
	uint32_t		vga_60fps_gop_opt_1;	// 0x190
	uint32_t		vga_60fps_gop_opt_2;	// 0x194
	uint32_t		vga_60fps_gop_opt_3;	// 0x198
	uint32_t		vga_60fps_gop_opt_4;	// 0x19c
	uint32_t		vga_50fps_gop_opt_0;	// 0x1a0
	uint32_t		vga_50fps_gop_opt_1;	// 0x1a4
	uint32_t		vga_50fps_gop_opt_2;	// 0x1a8
	uint32_t		vga_50fps_gop_opt_3;	// 0x1ac
	uint32_t		vga_50fps_gop_opt_4;	// 0x1b0
}__attribute__((aligned,packed));

//~ SIZE_CHECK_STRUCT( mvr_config, 0x30 );

extern struct mvr_config mvr_config;

// This is from 5D2, not used here.
/*
 *
 * State information is in this structure.  A pointer to the global
 * object is at 0x1ee0.  It is of size 0x1b4.
 *
 * The state object is in 0x68a4.
 */
/*struct mvr_struct
{
	const char *		type;	 // "MovieRecorder" off 0
	uint32_t		off_0x04;
	uint32_t		task;	// off_0x08;
	uint32_t		off_0x0c;
	uint32_t		off_0x10;
	uint32_t		off_0x14;
	uint32_t		off_0x18;
	uint32_t		off_0x1c;
	uint32_t		off_0x20;
	uint32_t		off_0x24;
	uint32_t		off_0x28;
	uint32_t		off_0x2c;
	uint32_t		off_0x30;
	uint32_t		off_0x34;
	uint32_t		off_0x38;
	uint32_t		off_0x3c;
	uint32_t		off_0x40;
	uint32_t		off_0x44;
	uint32_t		off_0x48;
	uint32_t		off_0x4c;
	uint32_t		off_0x50;
	uint32_t		off_0x54;
	uint32_t		off_0x58;
	uint32_t		off_0x5c;
	uint32_t		off_0x60;
	uint32_t		off_0x64;
	uint32_t		off_0x68;
	uint32_t		off_0x6c;
	uint32_t		off_0x70;
	uint32_t		off_0x74;
	uint32_t		off_0x78;
	uint32_t		off_0x7c;
	uint32_t		off_0x80;
	uint32_t		off_0x84;
	uint32_t		off_0x88;
	uint32_t		off_0x8c;
	uint32_t		off_0x90;
	uint32_t		off_0x94;
	uint32_t		off_0x98;
	uint32_t		off_0x9c;
	uint32_t		off_0xa0;
	uint32_t		off_0xa4;
	uint32_t		off_0xa8;
	uint32_t		off_0xac;
	uint32_t		off_0xb0;
	uint32_t		off_0xb4;
	uint32_t		off_0xb8;
	uint32_t		off_0xbc;
	uint32_t		off_0xc0;
	uint32_t		off_0xc4;
	uint32_t		off_0xc8;
	uint32_t		off_0xcc;
	uint32_t		off_0xd0;
	uint32_t		off_0xd4;
	uint32_t		off_0xd8;
	uint32_t		off_0xdc;
	uint32_t		off_0xe0;
	uint32_t		off_0xe4;
	uint32_t		off_0xe8;
	uint32_t		off_0xec;
	uint32_t		off_0xf0;
	uint32_t		off_0xf4;
	uint32_t		off_0xf8;
	uint32_t		off_0xfc;
	uint32_t		off_0x100;
	uint32_t		off_0x104;
	uint32_t		off_0x108;
	uint32_t		off_0x10c;
	uint32_t		off_0x110;
	uint32_t		off_0x114;
	uint32_t		off_0x118;
	uint32_t		off_0x11c;
	uint32_t		off_0x120;
	uint32_t		off_0x124;
	uint32_t		off_0x128;
	uint32_t		off_0x12c;
	uint32_t		off_0x130;
	uint32_t		off_0x134;
	uint32_t		off_0x138;
	uint32_t		off_0x13c;
	uint32_t		is_vga;	// 0==1920, 1==640 off_0x140;
	uint32_t		off_0x144;
	uint32_t		off_0x148;
	uint32_t		fps;		// 30, off_0x14c;
	uint32_t		width;		// off_0x150;
	uint32_t		height;		// off_0x154;
	uint32_t		audio_rec;	// off_0x158;
	uint32_t		auido_channels;	// 2 or 0, off_0x15c;
	uint32_t		audio_rate;	// 44100 or 0, off_0x160;
	uint32_t		off_0x164;
	uint32_t		off_0x168;
	uint32_t		off_0x16c;
	uint32_t		off_0x170;
	uint32_t		off_0x174;
	uint32_t		off_0x178;
	uint32_t		off_0x17c;
	uint32_t		off_0x180;
	uint32_t		off_0x184;
	uint32_t		off_0x188;
	uint32_t		off_0x18c;
	uint32_t		bit_rate; // off_0x190;
	uint32_t		off_0x194;
	uint32_t		off_0x198;
	uint32_t		off_0x19c;
	uint32_t		off_0x1a0;
	uint32_t		off_0x1a4;
	uint32_t		off_0x1a8;
	uint32_t		off_0x1ac;
	uint32_t		off_0x1b0;
	uint32_t		off_0x1b4;
	uint32_t		off_0x1b8;
	uint32_t		off_0x1bc;
	uint32_t		off_0x1c0;
	uint32_t		off_0x1c4;
	uint32_t		off_0x1c8;
	uint32_t		off_0x1cc;
	uint32_t		off_0x1d0;
	uint32_t		off_0x1d4;
	uint32_t		off_0x1d8;
	uint32_t		off_0x1dc;
	uint32_t		off_0x1e0;
	uint32_t		off_0x1e4;
	uint32_t		off_0x1e8;
	uint32_t		off_0x1ec;
	uint32_t		off_0x1f0;
	uint32_t		off_0x1f4;
	uint32_t		off_0x1f8;
	uint32_t		off_0x1fc;
};

SIZE_CHECK_STRUCT( mvr_struct, 512 );*/

#endif /* __PLATFORM_MVR_H__ */
