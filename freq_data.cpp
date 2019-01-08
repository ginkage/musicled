#include "freq_data.h"

#include <math.h>

// "Saw" function with specified range, peaks at (p / 2).
static inline double saw(double val, double p)
{
    double x = val / p;
    return p * fabs(x - floor(x + 0.5));
}

FreqData::FreqData(int n1, unsigned int rate)
    : color(n1)
    , note(n1)
    , x(n1)
    , left_amp(n1)
    , right_amp(n1)
{
    double maxFreq = 2 * n1;
    double minFreq = rate / maxFreq;
    double base = log(pow(2, 1.0 / 12.0));
    double fcoef = pow(2, 57.0 / 12.0) / 440.0; // Frequency 440 is a note number 57 = 12 * 4 + 9

    // Notes in [36, 108] range, i.e. 6 octaves
    minK = ceil(exp(35 * base) / (minFreq * fcoef));
    maxK = ceil(exp(108 * base) / (minFreq * fcoef));

    for (int k = 1; k < n1; k++) {
        double frequency = k * minFreq;
        double fnote = log(frequency * fcoef) / base; // note = 12 * Octave + Note
        double spectre = fmod(fnote, 12); // spectre is within [0, 12)
        double R = saw(spectre - 6, 12); // Peaks at C (== 0)
        double G = saw(spectre - 10, 12); // Peaks at E (== 4)
        double B = saw(spectre - 2, 12); // Peaks at G# (== 8)
        double mn = saw(spectre - 2, 4); // Minimum of them is also periodic

        // Technically, the formula for every component is:
        // Result == 255 * (C - Min) / (Max - Min),
        // where Min and Max are the smallest and the biggest of { R, G, B },
        // but Min is periodic, and (Max - Min) == 4, a constant.
        Color c;
        c.r = (int)((R - mn) * 63.75 + 0.5);
        c.g = (int)((G - mn) * 63.75 + 0.5);
        c.b = (int)((B - mn) * 63.75 + 0.5);

        color[k] = c;
        note[k] = fnote;
    }
}
