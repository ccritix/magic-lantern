// misc functions specific to 60D/109

#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <config.h>
#include <consts.h>
#include <lens.h>
#include <version.h>

struct battery_info {
	int num_of_batt; // 1 if battery is in the body, ? if grip attached
	int level;       // 0-100%
	int performance; // battery performancee 0?,1,2,3
	int expo;        // expo taken with this charge
	uint32_t serial; // serial number
	int num_of_hist; // number of registered batterys
	int act_hist;    // the actual one from the registered, 0: if the battery is not registered
	char name[6];    // LP-E6, ???
};

struct battery_history {
  uint32_t serial;   // serial number
  int level;         // 0-100%
  int year;          // year-1900  |
  int month;         // month-1    | the date when the camera last sees the battery
  int day;           // day        |
};

struct battery_history bat_hist[6];
struct battery_info bat_info;

CONFIG_INT("battery.drain.rate.rev", battery_seconds_same_level_ok, 0);
int battery_seconds_same_level_tmp = 0;
int battery_level_transitions = 0;

PROP_HANDLER(PROP_BATTERY_REPORT) // also in memory address 7AF60 length 96 bytes
{
    bat_info.level = buf[1] & 0xff;
    bat_info.performance = (buf[1] >> 8) & 0xff;
    bat_info.serial = (buf[5] & 0xff000000) + SWAP_ENDIAN(buf[6] << 8);
    bat_info.num_of_batt = buf[0];
    bat_info.expo = (buf[2] >> 8) & 0xffff; //expo taken with the battery 
    // from buf[2] >> 24 : battery name (byte 11-...) LP-E6 or ???
}

PROP_HANDLER(PROP_BATTERY_HISTORY) // also in memory address 7AFC0 length 76 bytes
{
    bat_info.num_of_hist = buf[0];
    bat_info.act_hist = 0;
    for (int i=0;i<MIN(bat_info.num_of_hist,6);i++) 
    {
        bat_hist[i].serial = buf[1+i*3];
        if (bat_hist[i].serial == bat_info.serial) bat_info.act_hist = i+1;
        bat_hist[i].level = buf[2+i*3] & 0xffff;
        bat_hist[i].year = (buf[2+i*3] >> 16);
        bat_hist[i].month = buf[3+i*3] & 0xffff;
        bat_hist[i].day = (buf[3+i*3] >> 16);
    }
}

void display_shooting_info() // called from debug task
{
	if (lv) return;
	
    uint32_t fnt;
    int bg;
    int col_bg = bmp_getpixel(1,1);;
    int col_field = bmp_getpixel(690,30);

#ifdef DISPLAY_HEADER_FOOTER_INFO
    extern int header_left_info;
    extern int header_right_info;
    extern int footer_left_info;
    extern int footer_right_info;
    char adate[11];
    char info[63];

//    bmp_fill(col_bg,28,3,694,21);
//    bmp_fill(col_bg,28,459,694,21);

    if (header_left_info==3 || header_right_info==3 || footer_left_info==3 || footer_right_info==3) 
    {
        struct tm now;
        LoadCalendarFromRTC( &now );
// need to find the date format settings and use it here
//        snprintf(adate, sizeof(adate), "%02d.%02d.%4d", (now.tm_mon+1),now.tm_mday,(now.tm_year+1900));
//        snprintf(adate, sizeof(adate), "%02d.%02d.%4d", now.tm_mday,(now.tm_mon+1),(now.tm_year+1900));
        snprintf(adate, sizeof(adate), "%4d.%02d.%02d", (now.tm_year+1900),(now.tm_mon+1),now.tm_mday);
    }
    
    fnt = FONT(FONT_MED, COLOR_FG_NONLV, col_bg); 
    if (header_left_info>0) 
        bmp_printf(fnt, 21, 2, (
            header_left_info==1 ? artist_name:
            header_left_info==2 ? copyright_info:
            header_left_info==3 ? adate:
            header_left_info==4 ? lens_info.name:
            header_left_info==5 ? build_version: 
            "")
        );
    if (footer_left_info>0) 
        bmp_printf(fnt, 21, 459, ( 
            footer_left_info==1 ? artist_name:
            footer_left_info==2 ? copyright_info:
            footer_left_info==3 ? adate:
            footer_left_info==4 ? lens_info.name:
            footer_left_info==5 ? build_version: 
            "") 
    );
    if (header_right_info>0) 
    {
        snprintf(info, sizeof(info), "%s", (
            header_right_info==1 ? artist_name:
            header_right_info==2 ? copyright_info:
            header_right_info==3 ? adate:
            header_right_info==4 ? lens_info.name:
            header_right_info==5 ? build_version: 
            ""));
        bmp_printf(fnt, 698-strlen(info) * font_med.width, 2, info);
    }
    
    if (footer_right_info>0) {
        snprintf(info, sizeof(info), "%s", (
            footer_right_info==1 ? artist_name:
            footer_right_info==2 ? copyright_info:
            footer_right_info==3 ? adate:
            footer_right_info==4 ? lens_info.name:
            footer_right_info==5 ? build_version: 
            ""));
        bmp_printf(fnt, 698-strlen(info) * font_med.width, 459, info);
    }
#endif

    if (lens_info.raw_iso == 0) // ISO: AUTO
     {
        fnt = FONT(FONT_MED, COLOR_YELLOW, col_field);
        bmp_printf(fnt, 545, 105, "[%d-%d]", MAX((get_htp() ? 200 : 100), raw2iso(auto_iso_range >> 8)), raw2iso((auto_iso_range & 0xFF)));
    }

	if (lens_info.wb_mode == WB_KELVIN)
	{
        fnt = FONT(FONT_MED, COLOR_YELLOW, col_field);
		bmp_printf(fnt, 160, 278, "%5d", lens_info.kelvin);
	}

	if (lens_info.wbs_gm || lens_info.wbs_ba)
	{
		int ba = lens_info.wbs_ba;
	    fnt = FONT(FONT_LARGE, COLOR_YELLOW, col_field);
		int x = 268;
		if (ba) bmp_printf(fnt, x + 2 * font_med.width, 280, "%s%d", ba > 0 ? "A" : "B", ABS(ba));
		else    bmp_printf(fnt, x + 2 * font_med.width, 280, "  ");

		int gm = lens_info.wbs_gm;
		if (gm) bmp_printf(fnt, x, 280, "%s%d", gm > 0 ? "G" : "M", ABS(gm));
		else    bmp_printf(fnt, x, 280, "  ");
	}
	
	hdr_display_status(fnt);

    RedrawBatteryIcon();

	bmp_printf(fnt, 395, 305, get_mlu() ? "MLU" : "   ");

	display_lcd_remote_icon(555, 460);
	display_trap_focus_info();
}

void RedrawBatteryIcon()
{
    int col_field = bmp_getpixel(615,455);
    uint32_t fnt = FONT(FONT_MED, COLOR_FG_NONLV, col_field);
    bmp_printf(fnt, 235, 415, "%d%% ", GetBatteryLevel());

/*
    int batlev = bat_info.level;
    int col_field = bmp_getpixel(615,455);
    uint32_t fnt = FONT(FONT_LARGE, COLOR_FG_NONLV, col_field);

    int bg = bmp_getpixel(155,425);
    if (bg==COLOR_WHITE) // If battery level<10 the Canon icon is flashing. In the empty state we don't redraw our battery icon but the rest 
    {
        uint batcol,batfil;
        bmp_fill(col_field,DISPLAY_BATTERY_POS_X-4,DISPLAY_BATTERY_POS_Y+14,96,32); // clear the Canon battery icon
        
        if (batlev <= DISPLAY_BATTERY_LEVEL_2)
        {
            batcol = COLOR_RED;
        }
        else
        {
            batcol = COLOR_WHITE;
        }
        
        bmp_fill(batcol,DISPLAY_BATTERY_POS_X+10,DISPLAY_BATTERY_POS_Y,72,32); // draw the new battery icon
        bmp_fill(batcol,DISPLAY_BATTERY_POS_X,DISPLAY_BATTERY_POS_Y+8,12,16);
        bmp_fill(col_field,DISPLAY_BATTERY_POS_X+14,DISPLAY_BATTERY_POS_Y+4,64,24);
        
        if (batlev <= DISPLAY_BATTERY_LEVEL_2)
        {
            batcol = COLOR_RED;
        }
        else if (batlev <= DISPLAY_BATTERY_LEVEL_1)
        {
            batcol = COLOR_YELLOW;
        }
        else
        {
            batcol = COLOR_GREEN2;
        }
        
        batfil = batlev*56/100;
        bmp_fill(batcol,DISPLAY_BATTERY_POS_X+18+56-batfil,DISPLAY_BATTERY_POS_Y+8,batfil,16);
    }
    bmp_printf(fnt, DISPLAY_BATTERY_POS_X+10, DISPLAY_BATTERY_POS_Y+35, "%3d%%", batlev);
	if (bat_info.act_hist>0) 
		bmp_printf(FONT(FONT_LARGE, COLOR_YELLOW, col_field), DISPLAY_BATTERY_POS_X+84, DISPLAY_BATTERY_POS_Y+1, "%d", bat_info.act_hist);
	int x = DISPLAY_BATTERY_POS_X-15; int y = DISPLAY_BATTERY_POS_Y-16; int w = 12;
	bmp_fill((bat_info.performance<3 ? COLOR_GRAY50 : COLOR_GREEN2),x,y,w,w);
	bmp_fill((bat_info.performance<2 ? COLOR_GRAY50 : COLOR_GREEN2),x,y+4+w,w,w);
	bmp_fill((bat_info.performance<1 ? COLOR_GRAY50 : COLOR_GREEN2),x,y+8+2*w,w,w);
*/
}

int GetBatteryLevel()
{
    return bat_info.level;
}
int GetBatteryTimeRemaining()
{
	return battery_seconds_same_level_ok * bat_info.level;
}
int GetBatteryDrainRate() // percents per hour
{
	return 3600 / battery_seconds_same_level_ok;
}

// called every second
void RefreshBatteryLevel_1Hz()
{
	static int k = 0;
	k++;
	
	if (k % 10 == 0 &&
		lens_info.job_state == 0) // who knows what race conditions are here... I smell one :)
	{
		int x = 31;
		prop_request_change(PROP_BATTERY_REPORT, &x, 1); // see PROP_Request PROP_BATTERY_REPORT
	}
	
	msleep(50);
	
	// check how many seconds battery indicator was at the same percentage
	// this is a rough indication of how fast the battery is draining
	static int old_battery_level = -1;
	if (bat_info.level == old_battery_level)
	{
		battery_seconds_same_level_tmp++;
	}
	else
	{
		battery_level_transitions++;
		if (battery_level_transitions >= 2)
			battery_seconds_same_level_ok = battery_seconds_same_level_tmp;
		battery_seconds_same_level_tmp = 0;
	}
	old_battery_level = bat_info.level;
}

// gcc mempcy has odd alignment issues?
void
my_memcpy(
    void *       dest,
    const void *     src,
    size_t          len
)
{
    while( len-- > 0 )
        *(uint8_t*)dest++ = *(const uint8_t*)src++;
}
