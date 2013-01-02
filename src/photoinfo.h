// additional info on photo screen

	int col_bg;
	int col_field;
    
    col_bg = bmp_getpixel(10,1);
    col_field = bmp_getpixel(615,375);
    
#ifdef DISPLAY_HEADER_FOOTER_INFO
    extern int header_left_info;
    extern int header_right_info;
    extern int footer_left_info;
    extern int footer_right_info;
    char adate[11];
    char info[63];
    
    //bmp_fill(col_bg,28,3,694,20);
    //bmp_fill(col_bg,28,459,694,20);
    
    if (header_left_info==3 || header_right_info==3 || footer_left_info==3 || footer_right_info==3)
    {
        struct tm now;
        LoadCalendarFromRTC( &now );
        // need to find the date format settings and use it here
        //      snprintf(adate, sizeof(adate), "%2d.%2d.%4d", (now.tm_mon+1),now.tm_mday,(now.tm_year+1900));
        snprintf(adate, sizeof(adate), "%2d.%2d.%4d", now.tm_mday,(now.tm_mon+1),(now.tm_year+1900));
        //		snprintf(adate, sizeof(adate), "%4d.%2d.%2d", (now.tm_year+1900),(now.tm_mon+1),now.tm_mday);
    }
    
    fnt = FONT(FONT_MED, COLOR_FG_NONLV, col_bg);
    if (header_left_info>0)
        bmp_printf(fnt, 28, 2, (
                                header_left_info==1 ? artist_name:
                                header_left_info==2 ? copyright_info:
                                header_left_info==3 ? adate:
                                header_left_info==4 ? lens_info.name:
                                header_left_info==5 ? build_version:
                                "")
                   );
    if (footer_left_info>0)
        bmp_printf(fnt, 28, 459, (
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
        bmp_printf(fnt, 693-strlen(info) * font_med.width, 2, info);
    }
    
    if (footer_right_info>0)
    {
        snprintf(info, sizeof(info), "%s", (
                                            footer_right_info==1 ? artist_name:
                                            footer_right_info==2 ? copyright_info:
                                            footer_right_info==3 ? adate:
                                            footer_right_info==4 ? lens_info.name:
                                            footer_right_info==5 ? build_version:
                                            ""));
        bmp_printf(fnt, 693-strlen(info) * font_med.width, 459, info);
    }
#endif
    
#ifdef STROBO_READY_AND_WE_CAN_USE_IT
    if (flash_info.mode==STROBO_FLASH_MODE_MANUAL)
    {
        uint32_t fntl = FONT(FONT_LARGE, COLOR_YELLOW, col_field);
        fnt = FONT(FONT_SMALL, COLOR_CYAN, col_field);
        bmp_printf(fnt, 488, 188, "A");
        bmp_printf(fntl, 498, 185, "%3d", 1 << flash_info.group_a_output);
        bmp_printf(fnt, 556, 188, "B");
        bmp_printf(fntl, 564, 185, "%3d", 1 << flash_info.group_b_output );
        bmp_printf(fnt, 624, 188, "C");
        bmp_printf(fntl, 632, 185, "%3d", 1 << flash_info.group_c_output);
        bmp_fill(bmp_getpixel(1,1),486,212,212,6);
    }
    //~ bmp_printf(fnt, 400, 450, "Flash:%s",
    //~ strobo_firing == 0 ? " ON" :
    //~ strobo_firing == 1 ? "OFF" : "Auto"
    //~ strobo_firing < 2 && flash_and_no_flash ? "/T" : "  "
    //~ );
#endif
    
    if (avail_shot>999) // it is a Canon bug (only 3 digits), it can display on the other info screen four digit number
    {                   // but we can write 5 digits if necessary
        bmp_fill(col_field,540,384,152,72); // clear the "[999]"
        char msg[7];
        snprintf(msg, sizeof(msg), "[%5d%]", avail_shot);
        bfnt_puts(msg, 550 , 402, COLOR_FG_NONLV, col_field);
    }
    /*	bg = bmp_getpixel(28, 2);
     bmp_printf(FONT(FONT_MED, COLOR_FG_NONLV, bg), 28, 0, lens_info.name);
     bg = bmp_getpixel(28, 459);
     
     bmp_printf(FONT(FONT_MED, COLOR_FG_NONLV, bg), 28, 459, "%s", artist_name);
     bg = bmp_getpixel(693, 459);
     
     bmp_printf(FONT(FONT_MED, COLOR_FG_NONLV, bg), 693-strlen(copyright_info) * font_med.width, 459, copyright_info);*/
    
	bg = bmp_getpixel(590, 28);
	fnt = FONT(FONT_MED, COLOR_FG_NONLV, bg);
    
	if (lens_info.raw_iso == 0) // ISO: MAX AUTO
 	{
        int maxiso=(auto_iso_range %  0xFF) - (auto_iso_range >> 8);
        bmp_printf(fnt, 590, 28, "MAX:%d",raw2iso(maxiso) );
	}