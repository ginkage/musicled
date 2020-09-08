#include "daubechies8.h"

decomposition Daubechies8::forward(std::vector<double>& data)
{
    int length = data.size();
    int half = length >> 1;
    int mask = length - 1;
    std::vector<double> energy = std::vector<double>(half);
    std::vector<double> detail = std::vector<double>(half);

    for (int i = 0; i < half; ++i) {
        double e = 0, d = 0;
        for (int j = 0; j < 8; ++j) {
            double v = data[((i << 1) + j) & mask];
            // low pass filter for the energy (approximation)
            e += v * scalingDecom[j];
            // high pass filter for the details
            d += v * waveletDecom[j];
        }
        energy[i] = e;
        detail[i] = d;
    }

    return { energy, detail };
}
