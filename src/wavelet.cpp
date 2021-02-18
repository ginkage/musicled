#include "wavelet.h"

#include <cmath>

Wavelet::Wavelet(int size, int maxLevel)
    : levels(std::min(std::ilogb(size), maxLevel))
    , decomp(levels)
{
    int half = size / 2;
    for (int level = 0; level < levels; level++) {
        decomp[level] = { std::vector<float>(half), std::vector<float>(half) };
        half >>= 1;
    }
}

// 1-D forward transforms from time domain to all possible Hilbert domains
std::vector<decomposition>& Wavelet::decompose(std::vector<float>& data)
{
    std::vector<float>* prev = &data;
    for (auto& level : decomp) {
        forward(*prev, level);
        prev = &level.first;
    }
    return decomp;
}

void Wavelet::forward(std::vector<float>& data, decomposition& out)
{
    int length = data.size();
    int half = length >> 1;
    int mask = length - 1;
    std::vector<float>& energy = out.first;
    std::vector<float>& detail = out.second;

    for (int i = 0; i < half; ++i) {
        float e = 0, d = 0;
        for (int j = 0; j < 8; ++j) {
            float v = data[((i << 1) + j) & mask];
            // low pass filter for the energy (approximation)
            e += v * scalingDecom[j];
            // high pass filter for the details
            d += v * waveletDecom[j];
        }
        energy[i] = e;
        detail[i] = d;
    }
}
