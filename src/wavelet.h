#pragma once

#include <vector>

typedef std::pair<std::vector<float>, std::vector<float>> decomposition;

class Wavelet {
public:
    Wavelet(int size, int maxLevels);

    // 1-D forward transforms from time domain to all possible Hilbert domains
    std::vector<decomposition>& decompose(std::vector<float>& data);

protected:
    // 1-D forward transform from time domain to Hilbert domain
    void forward(std::vector<float>& data, decomposition& out);

private:
    int levels;
    std::vector<decomposition> decomp;

    float scalingDecom[8] { -0.010597401784997278f, 0.032883011666982945f, 0.030841381835986965f,
        -0.18703481171888114f, -0.02798376941698385f, 0.6308807679295904f, 0.7148465705525415f,
        0.23037781330885523f };

    float waveletDecom[8] { 0.23037781330885523f, -0.7148465705525415f, 0.6308807679295904f,
        0.02798376941698385f, -0.18703481171888114f, -0.030841381835986965f, 0.032883011666982945f,
        0.010597401784997278f };
};
