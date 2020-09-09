#pragma once

#include <vector>

typedef std::pair<std::vector<double>, std::vector<double>> decomposition;

class Wavelet {
public:
    Wavelet() {}

    // 1-D forward transforms from time domain to all possible Hilbert domains
    void decompose(std::vector<double>& data, std::vector<decomposition>& decomp);

    static std::vector<decomposition> alloc(int size, int maxLevels);

protected:
    // 1-D forward transform from time domain to Hilbert domain
    virtual void forward(std::vector<double>& data, decomposition& out) = 0;
};
