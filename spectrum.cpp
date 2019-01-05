#include "spectrum.h"

#include <algorithm>
#include <math.h>

#define N 2048

Spectrum::Spectrum(GlobalState* state)
    : global(state)
    , audio(state)
    , freq(N / 2, audio.rate)
    , left(N)
    , right(N)
{
}

FreqData& Spectrum::process()
{
    left.read(audio.left);
    right.read(audio.right);

    fftw_complex* outl = left.execute();
    fftw_complex* outr = right.execute();

    int minK = freq.minK, maxK = freq.maxK;
    int maxF = freq.minK;
    double maxAmp = 0;

    for (int k = minK; k < maxK; k++) {
        double ampl = freq.left_amp[k] = hypot(outl[k][0], outl[k][1]);
        double ampr = freq.right_amp[k] = hypot(outr[k][0], outr[k][1]);
        double amp = std::max(ampl, ampr);
        if (amp > maxAmp) {
            maxAmp = amp;
            maxF = k;
        }
    }

    global->cur_Color = freq.color[maxF];
    return freq;
}

void Spectrum::start_input() { audio.start_thread(); }

void Spectrum::stop_input() { audio.join_thread(); }
