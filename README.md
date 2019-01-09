# Music-driven LED strip visuzlizer

Simple (like, really simple) Linux program that takes audio input from ALSA and sets an LED strip color in sync with the music.

## Overview

The idea is simple: take audio stream (using ALSA), put it through FFT (using [FFTW](http://www.fftw.org/)), take the "loudest" frequency (trivial), assign a color to it (using [Chromatic scale](https://en.wikipedia.org/wiki/Chromatic_scale), with all [12 notes](https://www.youtube.com/watch?v=IT9CPoe5LnM)), send that color to the LED strip (I'm using controllers flashed with [ESPurna](https://github.com/xoseperez/espurna)) or a few.

Now, do that, and also visualize the audio spectrum. 60 times per second. On a Raspberry Pi Zero. In C++.

OK, maybe it's not that impressive, but it sure is fun, and makes a good Christmas tree decoration that blinks in sync with your festive tunes.
