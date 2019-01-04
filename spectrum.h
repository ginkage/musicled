#ifndef __MUSICLED_SPECTRUM_H__
#define __MUSICLED_SPECTRUM_H__

#include "alsa_input.h"
#include "fft_data.h"
#include "freq_data.h"
#include "global_state.h"

class Spectrum {
public:
    Spectrum(GlobalState* state);
    FreqData& process();
    void start_input();
    void stop_input();

private:
    GlobalState* global;
    AlsaInput audio;
    FreqData freq;
    FftData left, right;
};

#endif
