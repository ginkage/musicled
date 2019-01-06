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
}

FreqData& Spectrum::process()
{
    left.read();
    right.read();

    fftw_complex* outl = left.execute();
    fftw_complex* outr = right.execute();

    std::vector<double>& left_amp = freq.left_amp;
    std::vector<double>& right_amp = freq.right_amp;
    int minK = freq.minK, maxK = freq.maxK;
    int maxF = freq.minK;
    double maxAmp = 0;

    for (int k = minK; k < maxK; k++) {
        double ampl = left_amp[k] = hypot(outl[k][0], outl[k][1]);
        double ampr = right_amp[k] = hypot(outr[k][0], outr[k][1]);
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
