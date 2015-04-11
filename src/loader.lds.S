/** \file
 * Linker script for magiclantern firmware.
 *
 * This is used for the runtime code, not the reboot wrapper script.
 *
 * The ARM is not running with any meaningful protection domains,
 * so we smoosh everything in together in the text segment.
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

MEMORY 
{
    init    (rwx) : ORIGIN = 0x00004200, LENGTH = 0x00000200
    ram     (rwx) : ORIGIN = 0x00004400, LENGTH = 0x00000E00
    code    (rwx) : ORIGIN = 0x00005200, LENGTH = 0x00003E00
}

SECTIONS
{
	.init : {
        __code_start = .;
		*(.init)
	} > init
    
	.data : {
        _data_start = .;
		*(.data)
        _data.end = .;
	} > ram
    
	.bss : {
        _bss_start = .;
        *(.bss)
        _bss_end = .;
    } > ram
    
    .ARM.exidx   : { 
        __exidx_start = .;
        *(.ARM.exidx*) 
        __exidx_end = .;
    } > code

	.text : {
		*(.text)
	} > code
    
	.rodata : {
        __rodata_start = .;
		*(.rodata)
        __rodata_end = .;
        
        . = ALIGN(64);
        __code_end = .;
        __kernel_start = .;
	} > code

    
}
