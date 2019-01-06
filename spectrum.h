#pragma once

#include "alsa_input.h"
#include "fft_data.h"
#include "freq_data.h"
#include "global_state.h"

class Spectrum {
public:
    Spectrum(GlobalState* state);
    ~Spectrum();
    FreqData& process();

private:
    GlobalState* global;
    AlsaInput audio;
    FreqData freq;
    FftData left, right;
};
