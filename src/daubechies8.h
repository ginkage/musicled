#include <cmath>
#include <vector>

typedef std::pair<std::vector<double>, std::vector<double>> decomposition;

class Daubechies8 {
public:
    Daubechies8() {}

    // 1-D forward transforms from time domain to all possible Hilbert domains
    std::vector<decomposition> decompose(std::vector<double>& data, int maxLevel)
    {
        int levels = std::min(std::ilogb(data.size()), maxLevel);
        std::vector<decomposition> matDecomp(levels);
        std::vector<double>* prev = &data;

        for (int level = 0; level < levels; level++) {
            prev = &(matDecomp[level] = forward(*prev)).first;
        }

        return matDecomp;
    }

private:
    double scalingDecom[8] { -0.010597401784997278, 0.032883011666982945, 0.030841381835986965,
        -0.18703481171888114, -0.02798376941698385, 0.6308807679295904, 0.7148465705525415,
        0.23037781330885523 };

    double waveletDecom[8] { 0.23037781330885523, -0.7148465705525415, 0.6308807679295904,
        0.02798376941698385, -0.18703481171888114, -0.030841381835986965, 0.032883011666982945,
        0.010597401784997278 };

    // 1-D forward transform from time domain to Hilbert domain
    decomposition forward(std::vector<double>& data)
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
};
