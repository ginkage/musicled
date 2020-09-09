#pragma once

#include "alsa_input.h"
#include "beat_dwt.h"
#include "fft_data.h"
#include "freq_data.h"
#include "global_state.h"
#include "thread_sync.h"

#include <memory>

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
    std::shared_ptr<ThreadSync> sync;
    AlsaInput audio; // Audio input thread
    std::shared_ptr<FreqData> freq; // Precomputed per-frequency data
    FftData fft; // FFTW3 computations for the left and right channels
    BeatDwt beat; // Beat detector thread
};
