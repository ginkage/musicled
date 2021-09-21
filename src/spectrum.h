#pragma once

#include "audio_input.h"
#include "beat_detect.h"
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
    std::unique_ptr<AudioInput> audio; // Audio input thread
    std::shared_ptr<FreqData> freq; // Precomputed per-frequency data
    FftData fft; // FFTW3 computations for the left and right channels
    BeatDetect beat; // Beat detector thread

    using Timestamp = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::steady_clock::duration;
    SlidingMedian<unsigned long, Timestamp, Duration> slide;
};
