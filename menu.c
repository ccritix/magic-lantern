/** \file
 * Magic Lantern GUI
 */
/*
 * Copyright (C) 2009 Trammell Hudson <hudson+ml@osresearch.net>
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


static struct semaphore * menu_sem;
static int draw_event;

static void
draw_version( void )
{
	bmp_printf(
		FONT( FONT_SMALL, COLOR_WHITE, COLOR_BLUE ),
		0, 32,
		"Magic Lantern Firmware version %s (%s)\nBuilt on%s by %s\n%s",
		build_version,
		build_id,
		build_date,
		build_user,
		"http://magiclantern.wikia.com/"
	);

/*
	int y = 200;
	struct config * config = global_config;
	bmp_printf( FONT_SMALL, 0, y, "Config: %x", (unsigned) global_config );
	y += font_small.height;

	while( config )
	{
		bmp_printf( FONT_SMALL, 0, y, "'%s' => '%s'", config->name, config->value );
		config = config->next;
		y += font_small.height;
	}
*/
}


static unsigned last_menu_event;
static struct gui_task * menu_task_ptr;
static struct menu * menus;


void
menu_print(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"%s",
		(const char*) priv
	);
}


static struct menu *
menu_find_by_name(
	const char *		name
)
{
	take_semaphore( menu_sem, 0 );

	struct menu *		menu = menus;

	for( ; menu ; menu = menu->next )
	{
		if( streq( menu->name, name ) )
		{
			give_semaphore( menu_sem );
			return menu;
		}

		// Stop just before we get to the end
		if( !menu->next )
			break;
	}

	// Not found; create it
	struct menu * new_menu = malloc( sizeof(*new_menu) );
	if( !new_menu )
	{
		give_semaphore( menu_sem );
		return NULL;
	}

	new_menu->name		= name;
	new_menu->prev		= menu;
	new_menu->next		= NULL; // Inserting at end
	new_menu->children	= NULL;

	// menu points to the last entry or NULL if there are none
	if( menu )
	{
		// We are adding to the end
		menu->next		= new_menu;
		new_menu->selected	= 0;
	} else {
		// This is the first one
		menus			= new_menu;
		new_menu->selected	= 1;
	}

	give_semaphore( menu_sem );
	return new_menu;
}


void
menu_add(
	const char *		name,
	struct menu_entry *	new_entry,
	int			count
)
{
#if 1
	// Walk the menu list to find a menu
	struct menu *		menu = menu_find_by_name( name );
	if( !menu )
		return;

	take_semaphore( menu_sem, 0 );

	struct menu_entry *	head = menu->children;
	if( !head )
	{
		// First one -- insert it as the selected item
		head = menu->children	= new_entry;
		new_entry->next		= NULL;
		new_entry->prev		= NULL;
		new_entry->selected	= 1;
		new_entry++;
		count--;
	}

	// Find the end of the entries on the menu already
	while( head->next )
		head = head->next;

	while( count-- )
	{
		new_entry->selected	= 0;
		new_entry->next		= head->next;
		new_entry->prev		= head;
		head->next		= new_entry;

		head			= new_entry;
		new_entry++;
	}
	give_semaphore( menu_sem );
#else
	// Maybe later...
	struct menu_entry * child = head->child;
	if( !child )
	{
		// No other child entries; add this one
		// and select it
		new_entry->highlighted	= 1;
		new_entry->prev		= NULL;
		new_entry->next		= NULL;
		head->child		= new_entry;
		return;
	}

	// Walk the child list to find the end
	while( child->next )
		child = child->next;

	// Push the new entry onto the end of the list
	new_entry->selected	= 0;
	new_entry->prev		= child;
	new_entry->next		= NULL;
	child->next		= new_entry;
#endif
}



void
menu_display(
	struct menu_entry *	menu,
	int			x,
	int			y,
	int			selected
)
{
	while( menu )
	{
		menu->display(
			menu->priv,
			x,
			y,
			menu->selected
		);

		y += font_large.height;
		menu = menu->next;
	}
}


void
menus_display(
	struct menu *		menu,
	int			orig_x,
	int			y
)
{
	int			x = orig_x;

	for( ; menu ; menu = menu->next )
	{
		unsigned fontspec = FONT(
			FONT_MED,
			COLOR_YELLOW,
			menu->selected ? 0x7F : COLOR_BG
		);
		bmp_printf( fontspec, x, y, "%6s", menu->name );
		x += fontspec_font( fontspec )->width * 6;

		if( menu->selected )
			menu_display(
				menu->children,
				orig_x,
				y + fontspec_font( fontspec )->height + 4,
				1
			);
	}
}


void
menu_entry_select(
	struct menu *	menu
)
{
	if( !menu )
		return;

	struct menu_entry * entry = menu->children;

	for( ; entry ; entry = entry->next )
	{
		if( entry->selected )
			break;
	}

	if( !entry || !entry->select )
		return;

	entry->select( entry->priv );
}

/** Scroll side to side in the list of menus */
void
menu_move(
	struct menu *		menu,
	int			direction
)
{
	if( !menu )
		return;

	// Deselect the current one
	menu->selected		= 0;

	if( direction < 0 )
	{
		if( menu->prev )
			menu = menu->prev;
		else {
			// Go to the last one
			while( menu->next )
				menu = menu->next;
		}
	} else {
		if( menu->next )
			menu = menu->next;
		else {
			// Go to the first one
			while( menu->prev )
				menu = menu->prev;
		}
	}

	// Select the new one (which might be the same)
	menu->selected		= 1;
}


/** Scroll up or down in the currently displayed menu */
void
menu_entry_move(
	struct menu *		menu,
	int			direction
)
{
	if( !menu )
		return;

	struct menu_entry *	entry = menu->children;

	for( ; entry ; entry = entry->next )
	{
		if( entry->selected )
			break;
	}

	// Nothing selected?
	if( !entry )
		return;

	// Deslect the current one
	entry->selected = 0;

	if( direction < 0 )
	{
		// First and moving up?
		if( entry->prev )
			entry = entry->prev;
		else {
			// Go to the last one
			while( entry->next )
				entry = entry->next;
		}
	} else {
		// Last and moving down?
		if( entry->next )
			entry = entry->next;
		else {
			// Go to the first one
			while( entry->prev )
				entry = entry->prev;
		}
	}

	// Select the new one, which might be the same as the old one
	entry->selected = 1;
}


static int
menu_handler(
	void *			priv,
	gui_event_t		event,
	int			arg2,
	int			arg3
)
{
	// Check if we should stop displaying
	if( !gui_show_menu
	|| event == TERMINATE_WINSYS
	|| event == DELETE_DIALOG_REQUEST )
	{
		DebugMsg( DM_MAGIC, 3, "Menu task shutting down: %d", event );
		//bmp_fill( COLOR_EMPTY, 90, 90, 720-180, 480-180 );
		return 1;
	}

	static uint32_t events[ MAX_GUI_EVENTS ][4];

	// Ignore periodic events
	if( event == GUI_TIMER )
		return 0;

	// Store the event in the log
	events[ last_menu_event ][0] = event;
	events[ last_menu_event ][1] = arg2;
	events[ last_menu_event ][2] = arg3;
	last_menu_event = (last_menu_event + 1) % MAX_GUI_EVENTS;

	if( draw_event )
		bmp_printf( FONT_SMALL, 400, 40,
			"event %08x",
			event
		);

	// Find the selected menu
	struct menu * menu = menus;
	for( ; menu ; menu = menu->next )
		if( menu->selected )
			break;

	switch( event )
	{
	case INITIALIZE_CONTROLLER:
		DebugMsg( DM_MAGIC, 3, "Menu task INITIALIZE_CONTROLLER" );
		last_menu_event = 0;
		return 0;

	case GOT_TOP_OF_CONTROL:
		DebugMsg( DM_MAGIC, 3, "Menu task GOT_TOP_OF_CONTROL" );
		goto redraw_dialog;

	case PRESS_JOY_UP:
	case ELECTRONIC_SUB_DIAL_LEFT:
		menu_entry_move( menu, -1 );
		break;

	case PRESS_JOY_DOWN:
	case ELECTRONIC_SUB_DIAL_RIGHT:
		menu_entry_move( menu, 1 );
		break;

	case PRESS_JOY_RIGHT:
	case DIAL_RIGHT:
		menu_move( menu, 1 );
		goto redraw_dialog;

	case PRESS_JOY_LEFT:
	case DIAL_LEFT:
		menu_move( menu, -1 );
		goto redraw_dialog;

	case PRESS_SET_BUTTON:
		menu_entry_select( menu );
		break;

	default:
		return 0;
	}
		
	// Something happened
	menus_display( menus, 100, 100 );
	return 0;

redraw_dialog:
	bmp_fill( COLOR_BG, 90, 90, 720-180, 480-180 );
	menus_display( menus, 100, 100 );
	return 0;
}





void
menu_init( void )
{
	gui_show_menu = 0;
	menus = NULL;
	menu_task_ptr = NULL;
	menu_sem = create_named_semaphore( "menus", 1 );

	menu_find_by_name( "Audio" );
	menu_find_by_name( "Video" );
	menu_find_by_name( "Brack" );
	menu_find_by_name( "Focus" );
	menu_find_by_name( "Games" );
	menu_find_by_name( "Debug" );

/*
	bmp_printf( FONT_LARGE, 0, 40, "Yes, use this battery" );
	gui_control( ELECTRONIC_SUB_DIAL_RIGHT, 1, 0 );
	msleep( 2000 );
	gui_control( PRESS_SET_BUTTON, 1, 0 );
	msleep( 2000 );

	// Try to defeat the battery message
	//GUI_SetErrBattery( 1 );
	//msleep( 100 );
	//StopErrBatteryApp();

	msleep( 1000 );
*/

}

static void
menu_task( void )
{
	// menu_init is too early for loading config values
	draw_event = config_int( global_config, "debug.draw-event", 0 );

	while(1)
	{
		if( !gui_show_menu )
		{
			if( menu_task_ptr )
			{
				gui_task_destroy( menu_task_ptr );
				menu_task_ptr = 0;
			}

			msleep( 500 );
			continue;
		}

		if( gui_show_menu == 1 )
		{
			DebugMsg( DM_MAGIC, 3, "Creating menu task" );
			last_menu_event = 0;
			menu_task_ptr = gui_task_create( menu_handler, 0 );
			gui_show_menu = 2;
			draw_version();
		}

		msleep( 100 );
	}
}

TASK_CREATE( "menu_task", menu_task, 0, 0x1e, 0x1000 );
