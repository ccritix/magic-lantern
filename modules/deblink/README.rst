Blink decoder
=============

:Author: a1ex
:License: GPL
:Summary: Decode diagnostic messages sent by another camera via card led

If you have a camera that no longer boots, you may examine its startup process
by compiling the dm-spy-experiments branch with CONFIG_DEBUG_INTERCEPT_STARTUP_BLINK=y
in your Makefile.user. This will intercept all Canon diagnostic messages, and encode them as LED blinks.

This module decodes those messages (it runs from a second camera, which should be pointed at the card led of the first one).
It works best at 50/60 FPS.
