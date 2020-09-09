#include "spectrum.h"

Spectrum::Spectrum(GlobalState* state, int N)
    : global(state)
    , sync(new ThreadSync())
    , audio(state, sync)
    , freq(new FreqData(N, audio.get_rate()))
    , fft(N, audio.get_data())
    , beat(state, audio.get_data(), sync, freq, audio.get_rate(), 131072)
{
    audio.start_thread();
    beat.start_thread();
}

Spectrum::~Spectrum()
{
    beat.join_thread();
    sync->produce([] {}); // No-op to release the consumer thread
    audio.join_thread();
}

FreqData& Spectrum::process()
{
    // Compute FFT for both channels
    Sample* out = fft.execute();

    int maxF = freq->minK;
    double maxAmp = 0;

    // Find the loudest frequency
    for (int k = freq->minK; k < freq->maxK; k++) {
        double amp = freq->amp[k] = std::abs(out[k]);
        if (amp > maxAmp) {
            maxAmp = amp;
            maxF = k;
        }
    }

    // Set the global color accordingly
    global->cur_Color = freq->color[maxF];
    return *freq;
}
