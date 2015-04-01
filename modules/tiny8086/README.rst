Tiny 8086 emulator
==================

:Author: Adrian Cable
:Ported by: a1ex
:License: MIT
:Summary: 8086 emulator - runs old DOS games :)


This is a port of 8086tiny http://www.megalith.co.uk/8086tiny/ .
It's too slow for any real use, but it's a fun experiment.

You need these files on your card:

* tiny8086.mo in ML/MODULES
* bios and fd.img from the 8086tiny website, on the root directory

To build the desktop emulator: make -f Makefile.orig
