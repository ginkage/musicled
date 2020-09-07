#include <cmath>

#include "wavelet.h"

// 1-D forward transforms from time domain to all possible Hilbert domains
std::vector<decomposition> Wavelet::decompose(std::vector<double>& data, int maxLevel)
{
    int levels = std::min(std::ilogb(data.size()), maxLevel);
    std::vector<decomposition> matDecomp(levels);
    std::vector<double>* prev = &data;

    for (int level = 0; level < levels; level++) {
        prev = &(matDecomp[level] = forward(*prev)).first;
    }

    return matDecomp;
}
