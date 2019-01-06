#pragma once

#include "color.h"

#include <vector>

struct FreqData {
    FreqData(int n1, unsigned int rate);

    std::vector<Color> color;
    std::vector<double> note;
    std::vector<int> x;
    std::vector<double> left_amp;
    std::vector<double> right_amp;
    int minK, maxK;
};
