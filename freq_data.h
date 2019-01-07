#pragma once

#include "color.h"

#include <vector>

// Collection of precomputed per-frequency data for FFT output
struct FreqData {
    FreqData(int n1, unsigned int rate);

    std::vector<Color> color; // "Rainbow"-based note color
    std::vector<double> note; // Note "value" for the frequency, [0, 12)
    std::vector<int> x; // Horizontal position in the visualization
    std::vector<double> left_amp; // Magnitude for the left channel
    std::vector<double> right_amp; // Magnitude for the right channel
    int minK, maxK; // The range of "meaningful" frequencies
};
