/** \file
 * Magic Lantern menu help
 */
/*
 * Copyright (C) 2011 Alex Dumitrache <broscutamaker@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "dryos.h"
#include "version.h"
#include "bmp.h"
#include "gui.h"
#include "config.h"
#include "property.h"
#include "lens.h"
#include "font.h"
#include "menu.h"
#include "menuhelp.h"

extern int menu_help_active;
int current_page = 1;
extern int help_pages;
void draw_page_number(int page);

void 
draw_beta_warning()
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    if (page_number_active) draw_page_number(1);

//    bmp_printf(FONT_LARGE, 360 - font_large.width * 6, 50, "Magic Lantern");
    bfnt_puts("Magic Lantern", 242, 53, COLOR_FG_NONLV, COLOR_BG);

    bmp_printf(FONT_MED, 50, 150, "This is a development snapshot for testing purposes.");

    bmp_printf(FONT_MED, 50, 200, "   Please report all bugs at www.magiclantern.fm.   ");

    bmp_printf(FONT_MED, 50, 250, "      Be careful using it for production work.      ");

    bmp_printf(FONT_MED, 50, 300, "                       Enjoy!                       ");

    big_bmp_printf(FONT_MED,  10,  410,
        "Magic Lantern version : %s\n"
        "Mercurial changeset   : %s\n"
        "Built on %s by %s.",
        build_version,
        build_id,
        build_date,
        build_user);
}

void 
draw_404_page()
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);

    bfnt_puts("404 Undocumented Feature", 10, 20, COLOR_WHITE, COLOR_BLACK);
    
    bmp_printf(FONT_MED, 10, 100, "This feature is probably not yet documented.");
    bmp_printf(FONT_MED, 10, 120, "After all, we are programmers, not tech writers.");

    bmp_printf(FONT_MED, 10, 180, "But... you can simply try it and see what it does.");

    bmp_printf(FONT_MED, 10, 240, "Then, write a short paragraph to describe it,");
    bmp_printf(FONT_MED, 10, 260, "and we will include it in the user guide.");

    bmp_printf(FONT_MED, 10, 320, "Thanks!");

}

void 
draw_help_not_installed_page()
{
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);

    bfnt_puts("Help files not found", 10, 20, COLOR_WHITE, COLOR_BLACK);
    
    bmp_printf(FONT_MED, 10, 150, "Magic Lantern help files could not be found.              ");

    bmp_printf(FONT_MED, 10, 200, "Make sure all ML files are installed to your card.        ");

    bmp_printf(FONT_MED, 10, 250, "See http://wiki.magiclantern.fm/install for instructions. ");
}

void menu_help_show_page(int page)
{
    menu_help_active = 1;
    
#ifndef CONFIG_RELEASE_BUILD
    if (page == 1) { draw_beta_warning(); return; } // display this instead of the main About page
#endif

    if (page == 0) { draw_404_page(); return; } // help page not found
    if (page == -1) { draw_help_not_installed_page(); return; } // help page not found
    
    char path[100],rpath[30];
    struct bmp_file_t * doc = (void*) -1;

    snprintf(rpath, sizeof(rpath), CARD_DRIVE "ML/doc/page-%03d.vrm", page);
    if (load_vram(rpath)==-1)
    {
        snprintf(path, sizeof(path), CARD_DRIVE "ML/doc/page-%03d.bmh", page);
        doc = bmp_load(path, 1);
        if (!doc)
        {
            snprintf(path, sizeof(path), CARD_DRIVE "ML/doc/page-%03d.bmp", page);
            doc = bmp_load(path, 1);
        }

        if (doc)
        {
            bmp_draw_scaled_ex(doc, 0, 0, 720, 480, 0);

            extern int _bmp_draw_should_stop;
            if (!_bmp_draw_should_stop) save_vram(rpath);
            FreeMemory(doc);
        }
        else
        {
            clrscr();
            bmp_printf(FONT_MED, 0, 0, "Could not load help page %s.", path);
        }
    }
    if (page_number_active==1) draw_page_number(page);
}

void draw_page_number(int page)
{
    char pg[4];
    snprintf(pg, sizeof(pg), "%3d", page);
    bfnt_puts(pg, 650+(page<10 ? 14 : page<100 ? 4 : 0) , 2, COLOR_FG_NONLV, COLOR_BG);

}

void menu_help_redraw()
{
    BMP_LOCK( menu_help_show_page(current_page); );
}

void menu_help_next_page()
{
    current_page = mod(current_page, help_pages) + 1;
    menu_help_active = 1;
}

void menu_help_prev_page()
{
    current_page = mod(current_page - 2, help_pages) + 1;
    menu_help_active = 1;
}

void menu_help_go_to_page(int page)
{
    current_page = page;
    menu_help_active = 1;
}

void str_make_lowercase(char* s)
{
    while (*s) { *s = tolower(*s); s++; }
}

void menu_help_go_to_label(void* label, int delta)
{
    int page = 0; // if help page won't be found, will show 404
    if (is_menu_selected("Help")) page = 1; // don't show the 404 page in Help menu :P
    
    // hack: use config file routines to parse menu index file
    extern int config_file_size, config_file_pos;
    extern char* config_file_buf;
    config_file_buf = (void*)read_entire_file(CARD_DRIVE "ML/doc/menuidx.dat", &config_file_size);
    config_file_pos = 0;
    
    if (!config_file_size) page = -1; // show "help not found" warning

    char line_buf[ 100 ];
    
    // trim spaces
    char label_adj[100];
    snprintf(label_adj, sizeof(label_adj), "%s", label);
    while (label_adj[strlen(label_adj)-1] == ' ')
    {
        label_adj[strlen(label_adj)-1] = '\0';
    }
    str_make_lowercase(label_adj);

    while( read_line(line_buf, sizeof(line_buf) ) >= 0 )
    {
        char* name = line_buf+4;
        str_make_lowercase(name);
        int pagenum = atoi(line_buf);
        if(streq(name, label_adj))
        {
            page = pagenum;
        }
        help_pages = MAX(help_pages, pagenum);
    }
    free_dma_memory(config_file_buf);
    config_file_buf = 0;
    
    current_page = page;
    menu_help_active = 1;
}
