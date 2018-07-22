ISO registers
=============

:Author: a1ex
:License: GPL
:Summary: Access to ISO-related registers on 5D Mark III. Research only.
:Tags: ISO experiments

This is a research tool that lets you override ISO registers:

* CMOS gain
* ADTG gains (0x8882 ... 0x8888)
* ADTG black (0x8880)
* SaturateOffset (0xc0f0819c)
* 0xc0f37ae4, af0, afc, b08 (digital gain)
* c0f37ae0/aec/af8/b04 (offset for black + white levels)

Requires: a Magic Lantern core compiled with CONFIG_GDB=y.
Conflicts with: Dual ISO, Mini ISO, ADTG GUI.
