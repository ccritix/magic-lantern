TFT registers
=============

Lets you adjust TFT controller registers (reverse engineering).

Mirror/flip registers:

- 5D3: register 0x02, mask 0x03
- 60D, 600D: registers 0x10 and 0xF0, masks 0x01 and 0x80
- 650D, 700D: register 0x01, mask 0xC0
- 70D: register 0x06, mask 0x06

There are also registers for color adjustments, image alignment, scaling and others.

For other models:

1. find the stubs: http://www.magiclantern.fm/forum/index.php?topic=12177.0
2. document the registers: http://www.magiclantern.fm/forum/index.php?topic=21108.0

:Author: a1ex
:License: GPL
:Summary: Adjust TFT controller registers (SIO interface).
:Forum: http://www.magiclantern.fm/forum/index.php?topic=21108.0

