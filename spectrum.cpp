#include "spectrum.h"

#include <algorithm>
#include <math.h>

// "Saw" function with specified range
inline double saw(double val, double p)
{
    double x = val / p;
    return p * fabs(x - floor(x + 0.5));
}

Spectrum::Spectrum(GlobalState* state)
    : freq(new FreqData[N1])
    , global(state)
    , audio(state)
{
    double maxFreq = N;
    double minFreq = audio.rate / maxFreq;
    double base = log(pow(2, 1.0 / 12.0));
    double fcoef = pow(2, 57.0 / 12.0) / 440.0; // Frequency 440 is a note number 57 = 12 * 4 + 9

    // Notes in [36, 108] range, i.e. 6 octaves
    minK = ceil(exp(35 * base) / (minFreq * fcoef));
    maxK = ceil(exp(108 * base) / (minFreq * fcoef));

    for (int k = 1; k < N1; k++) {
        double frequency = k * minFreq;
        double note = log(frequency * fcoef) / base; // note = 12 * Octave + Note
        double spectre = fmod(note, 12); // spectre is within [0, 12)
        double R = saw(spectre - 6, 12);
        double G = saw(spectre - 10, 12);
        double B = saw(spectre - 2, 12);
        double mn = saw(spectre - 2, 4);

        Color c;
        c.r = (int)((R - mn) * 63.75 + 0.5);
        c.g = (int)((G - mn) * 63.75 + 0.5);
        c.b = (int)((B - mn) * 63.75 + 0.5);

        FreqData f;
        f.c = c;
        f.note = note;
        f.ic = (((long)c.r) << 16) + (((long)c.g) << 8) + ((long)c.b);

        freq[k] = f;
    }
}

Spectrum::~Spectrum() { delete[] freq; }

void Spectrum::process()
{
    left.read(audio.left);
    if (audio.channels == 2)
        right.read(audio.right);

    left.execute();
    if (audio.channels == 2)
        right.execute();

    fftw_complex* outl = left.out;
    fftw_complex* outr = right.out;
    int maxF = minK;
    double maxAmp = 0;

    for (int k = minK; k < maxK; k++) {
        double ampl = left.amp[k] = hypot(outl[k][0], outl[k][1]);
        double ampr = right.amp[k] = hypot(outr[k][0], outr[k][1]);
        double amp = std::max(ampl, ampr);
        if (amp > maxAmp) {
            maxAmp = amp;
            maxF = k;
        }
    }

    global->cur_Color = freq[maxF].c;
}

void Spectrum::start_input() { audio.start_thread(); }

void Spectrum::stop_input() { audio.join_thread(); }
