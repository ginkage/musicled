#include "daubechies4.h"

#include <cmath>

Daubechies4::Daubechies4()
{
    double sqrt3 = std::sqrt(3.0); // 1.7320508075688772
    double sqrt2 = std::sqrt(2.0); // 1.4142135623730951
    scalingDecom[0] = ((1.0 + sqrt3) / 4.0) / sqrt2; // s0
    scalingDecom[1] = ((3.0 + sqrt3) / 4.0) / sqrt2; // s1
    scalingDecom[2] = ((3.0 - sqrt3) / 4.0) / sqrt2; // s2
    scalingDecom[3] = ((1.0 - sqrt3) / 4.0) / sqrt2; // s3

    for (int i = 0; i < 4; i++) {
        if (i % 2 == 0)
            waveletDecom[i] = scalingDecom[3 - i];
        else
            waveletDecom[i] = -scalingDecom[3 - i];
    }
}

// 1-D forward transform from time domain to Hilbert domain
decomposition Daubechies4::forward(std::vector<double>& data)
{
    int length = data.size();
    int half = length >> 1;
    int mask = length - 1;
    std::vector<double> energy = std::vector<double>(half, 0);
    std::vector<double> detail = std::vector<double>(half, 0);

    for (int i = 0; i < half; ++i) {
        for (int j = 0; j < 4; ++j) {
            int k = ((i << 1) + j) & mask;
            // low pass filter for the energy (approximation)
            energy[i] += data[k] * scalingDecom[j];
            // high pass filter for the details
            detail[i] += data[k] * waveletDecom[j];
        }
    }

    return { energy, detail };
}