#pragma once

#include "alsa_input.h"
#include "fft_data.h"
#include "freq_data.h"
#include "global_state.h"

// Starts audio input thread and calculates current LED strip color.
// Object creation spawns a thread for audio input (using the global state to
// stop it), destruction joins it.
// The calculated color is stored in the global state.
class Spectrum {
public:
    Spectrum(GlobalState* state, int N);
    ~Spectrum();

    // Read the latest samples, perform FFT, analyze the result
    FreqData& process();

private:
    GlobalState* global; // Global state to store the current color
    AlsaInput audio; // Audio input thread
    FreqData freq; // Precomputed per-frequency data
    FftData left, right; // FFTW3 computations for the left and right channels
};
