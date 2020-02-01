#include "spectrum.h"

Spectrum::Spectrum(GlobalState* state, int N)
    : global(state)
    , audio(state)
    , freq(N, audio.get_rate())
    , fft(N, audio.get_data())
{
    audio.start_thread();
}

Spectrum::~Spectrum() { audio.join_thread(); }

FreqData& Spectrum::process()
{
    // First, read both channels
    fft.read();

    // Compute FFT for both of them
    Sample* out = reinterpret_cast<Sample*>(fft.execute());

    int maxF = freq.minK;
    double maxAmp = 0;

    // Find the loudest frequency
    for (int k = freq.minK; k < freq.maxK; k++) {
        double amp = freq.amp[k] = std::abs(out[k]);
        if (amp > maxAmp) {
            maxAmp = amp;
            maxF = k;
        }
    }

    // Set the global color accordingly
    global->cur_Color = freq.color[maxF];
    return freq;
}
