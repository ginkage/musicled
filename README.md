# Music-driven LED strip visualizer

Simple (like, really simple) Linux program that takes audio input from ALSA and sets an LED strip color in sync with the music.

## Overview

The idea is pretty common: take audio input stream (using ALSA), put it through FFT (using [FFTW](http://www.fftw.org/)), take the "loudest" frequency (trivial), assign a color to it (using [Chromatic scale](https://en.wikipedia.org/wiki/Chromatic_scale), with all [12 notes](https://www.youtube.com/watch?v=IT9CPoe5LnM)), send that color to an LED strip or a few (I'm using controllers flashed with [ESPurna](https://github.com/xoseperez/espurna)).

Now, do all that, and also visualize the audio spectrum. 60 times per second. On a Raspberry Pi Zero. In C++.

![What it looks like](img/screenshot.png)

OK, maybe it's not that impressive, but it sure is fun, and makes a good Christmas tree decoration that blinks in sync with your festive tunes, or a good party lighting if you happen to have some LED strips in your room, like I do.

## My Setup

The main use case for me is: there's a device that plays music (it could be a home theater receiver, or just the speakers plugged into my desktop PC), and by plugging a Raspberry Pi-based device into it (using a splitter cable or the "Zone B" feature) I can process that music and control the WiFi LED strip. The LED controller here is irrelevant: I have an H801 and a MagicHome ones, both flashed with ESPurna firmware, and both working equally well.

The device I use is more interesting: it's a Pi Zero bundled with an [AudioInjector Zero](https://www.kickstarter.com/projects/1250664710/audio-injector-zero-sound-card-for-the-raspberry-p) sound card and a [Waveshare 5-inch HDMI display](https://www.waveshare.com/5inch-hdmi-lcd-b.htm) connected to it. Fairly cheap setup that allows me to use music lights with any audio source. This program, though, should work just as well on any other Linux-based computer, ideally the one playing that music in the first place to avoid DAC-to-ADC double conversion.

## Notes

The main audio capture and processing core are loosely based on the [CAVA](https://github.com/karlstav/cava) project.
