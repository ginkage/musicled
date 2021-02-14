#include <cmath>

#include "wavelet.h"

std::vector<decomposition> Wavelet::alloc(int size, int maxLevel)
{
    int levels = std::min(std::ilogb(size), maxLevel);
    std::vector<decomposition> decomp(levels);
    int half = size / 2;
    for (int level = 0; level < levels; level++) {
        decomp[level] = { std::vector<double>(half), std::vector<double>(half) };
        half >>= 1;
    }
    return decomp;
}

// 1-D forward transforms from time domain to all possible Hilbert domains
void Wavelet::decompose(std::vector<double>& data, std::vector<decomposition>& decomp)
{
    std::vector<double>* prev = &data;
    for (auto& level : decomp) {
        forward(*prev, level);
        prev = &level.first;
    }
}
