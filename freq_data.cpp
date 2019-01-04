#include "freq_data.h"

#include <math.h>

// "Saw" function with specified range
inline double saw(double val, double p)
{
    double x = val / p;
    return p * fabs(x - floor(x + 0.5));
}

FreqData::FreqData(int n, unsigned int rate)
    : color(new Color[n])
    , ic(new unsigned long[n])
    , note(new double[n])
    , x(new int[n])
    , left_amp(new double[n])
    , right_amp(new double[n])
{
    double maxFreq = n;
    double minFreq = rate / maxFreq;
    double base = log(pow(2, 1.0 / 12.0));
    double fcoef = pow(2, 57.0 / 12.0) / 440.0; // Frequency 440 is a note number 57 = 12 * 4 + 9

    // Notes in [36, 108] range, i.e. 6 octaves
    minK = ceil(exp(35 * base) / (minFreq * fcoef));
    maxK = ceil(exp(108 * base) / (minFreq * fcoef));

    for (int k = 1, n1 = n / 2; k < n1; k++) {
        double frequency = k * minFreq;
        double fnote = log(frequency * fcoef) / base; // note = 12 * Octave + Note
        double spectre = fmod(fnote, 12); // spectre is within [0, 12)
        double R = saw(spectre - 6, 12);
        double G = saw(spectre - 10, 12);
        double B = saw(spectre - 2, 12);
        double mn = saw(spectre - 2, 4);

        Color c;
        c.r = (int)((R - mn) * 63.75 + 0.5);
        c.g = (int)((G - mn) * 63.75 + 0.5);
        c.b = (int)((B - mn) * 63.75 + 0.5);

        color[k] = c;
        note[k] = fnote;
        ic[k] = (((long)c.r) << 16) + (((long)c.g) << 8) + ((long)c.b);
    }
}

FreqData::~FreqData()
{
    delete[] color;
    delete[] ic;
    delete[] note;
    delete[] x;
    delete[] left_amp;
    delete[] right_amp;
}
