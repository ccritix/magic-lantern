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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/* this will include the linux kernel */
asm(
    ".section .rodata\n"
    ".word 0xF00DCAFE\n"
    ".word kernel_end - kernel_start\n"
    ".globl kernel_start\n"
    "kernel_start:\n"
    //".incbin \"../../xipImage\"\n"
    ".incbin \"../../zImage\"\n"
    "kernel_end:\n"
    ".globl kernel_end\n"
    
    ".word initrd_end - initrd_start\n"
    ".globl initrd_start\n"
    "initrd_start:\n"
    //".incbin \"../../initramfs.igz\"\n"
    ".incbin \"../../initrd.img\"\n"
    //".incbin \"../../rootfs.cpio.gz\"\n"
    //".incbin \"../../rootfs.ext2\"\n"
    "initrd_end:\n"
    ".globl initrd_end\n"
);
