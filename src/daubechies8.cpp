#include "daubechies8.h"

decomposition Daubechies8::forward(std::vector<double>& data)
{
    int length = data.size();
    int half = length >> 1;
    int mask = length - 1;
    std::vector<double> energy = std::vector<double>(half, 0);
    std::vector<double> detail = std::vector<double>(half, 0);

    for (int i = 0; i < half; ++i) {
        for (int j = 0; j < 8; ++j) {
            int k = ((i << 1) + j) & mask;
            // low pass filter for the energy (approximation)
            energy[i] += data[k] * scalingDecom[j];
            // high pass filter for the details
            detail[i] += data[k] * waveletDecom[j];
        }
    }

    return { energy, detail };
}
