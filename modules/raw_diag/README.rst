RAW Diagnostics
===============

:Author: a1ex
:License: GPL
:Summary: Technical analysis of raw image data. Noise, DR, SNR...

Technical analysis of the raw image data:

* Optical black analysis: mean, stdev, histogram, dynamic range.
* Dark frame analysis: same as OB, take a picture with lens cap on.
* SNR analysis: plot a SNR graph from a defocused image that covers 
  the entire tone range (from dark shadows to overexposed highlights).

TODO:

* noise autocorrelation, trend
* detailed raw histogram, zoom on shadow and highlight areas.
* SNR colors; option to reference the SNR graphs to the scene:
  http://theory.uchicago.edu/~ejm/pix/20d/tests/noise/DRwindow1d3.png
