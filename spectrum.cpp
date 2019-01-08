#include "spectrum.h"

#include <algorithm>
#include <math.h>

#define N 2048

Spectrum::Spectrum(GlobalState* state)
    : global(state)
    , audio(state)
    , freq(N / 2, audio.get_rate())
    , left(N, audio.get_left())
    , right(N, audio.get_right())
{
    audio.start_thread();
}

Spectrum::~Spectrum() { audio.join_thread(); }

FreqData& Spectrum::process()
{
    // First, read both channels
    left.read();
    right.read();

    // Compute FFT for both of them
    fftw_complex* outl = left.execute();
    fftw_complex* outr = right.execute();

    int maxF = freq.minK;
    double maxAmp = 0;

    // Find the loudest frequency
    for (int k = freq.minK; k < freq.maxK; k++) {
        double ampl = freq.left_amp[k] = hypot(outl[k][0], outl[k][1]);
        double ampr = freq.right_amp[k] = hypot(outr[k][0], outr[k][1]);
        double amp = std::max(ampl, ampr);
        if (amp > maxAmp) {
            maxAmp = amp;
            maxF = k;
        }
    }

    // Set the global color accordingly
    global->cur_Color = freq.color[maxF];
    return freq;
}
