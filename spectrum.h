#ifndef __MUSICLED_SPECTRUM_H__
#define __MUSICLED_SPECTRUM_H__

#include "audio_data.h"
#include "fft_data.h"
#include "freq_data.h"
#include "global_state.h"

class Spectrum {
public:
    Spectrum(global_state* state);
    ~Spectrum();
    void precalc();
    void process();
    void start_input();
    void stop_input();

public:
    freq_data* freq;
    FFTData left, right;
    int minK, maxK;

private:
    global_state *global;
    audio_data audio;
};

#endif
