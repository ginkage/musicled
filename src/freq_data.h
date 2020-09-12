#pragma once

#include "color.h"

#include <vector>

// Collection of precomputed per-frequency data for FFT output
struct FreqData {
    FreqData(int n1, unsigned int rate);

    // Note == (12 * Octave + Spectre), where Spectre is in [0, 12)
    std::vector<Color> color; // "Rainbow"-based note color
    std::vector<double> note; // Note "value" for the frequency
    std::vector<int> x; // Horizontal position in the visualization
    std::vector<double> amp; // Magnitude
    int minK, maxK; // The range of "meaningful" frequencies

    bool ready;
    std::vector<double> wx;
    std::vector<double> wy;
};
