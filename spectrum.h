#ifndef __MUSICLED_SPECTRUM_H__
#define __MUSICLED_SPECTRUM_H__

#include "audio_data.h"
#include "freq_data.h"
#include "fft_data.h"

class Spectrum {
public:
    Spectrum(audio_data* data);
    ~Spectrum();
    void precalc();
    void process();

public:
    audio_data* audio;
    freq_data* freq;
    FFTData left, right;
    int minK, maxK;
};

#endif
