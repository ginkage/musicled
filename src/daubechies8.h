#include <cmath>
#include <vector>

class Daubechies8 {
private:
    // The coefficients of the mother scaling (low pass filter) for decomposition.
    double scalingDecom[8] {
        -0.010597401784997278,
        0.032883011666982945,
        0.030841381835986965,
        -0.18703481171888114,
        -0.02798376941698385,
        0.6308807679295904,
        0.7148465705525415,
        0.23037781330885523,
    };

    // The coefficients of the mother wavelet (high pass filter) for decomposition.
    double waveletDecom[8];

    Daubechies8()
    {
        // building wavelet as orthogonal (orthonormal) space from
        // scaling coefficients (low pass filter). Have a look into
        // Alfred Haar's wavelet or the Daubechies Wavelet with 2
        // vanishing moments for understanding what is done here. ;-)
        for (int i = 0; i < 8; i++) {
            if (i % 2 == 0)
                waveletDecom[i] = scalingDecom[7 - i];
            else
                waveletDecom[i] = -scalingDecom[7 - i];
        }
    }

    std::vector<std::vector<double>> decompose(std::vector<double>& arrTime)
    {
        int length = arrTime.size();
        int levels = std::ilogb(length);
        std::vector<std::vector<double>> matDecomp(levels + 1);

        matDecomp[0] = arrTime;

        // 1-D forward transforms from time domain to all possible Hilbert domains
        int waveLength = length;
        for (int level = 1; level <= levels; level++) {
            // 1-D forward transform from time domain to Hilbert domain
            std::vector<double>& arrHilb = matDecomp[level - 1];
            std::vector<double>& arrTemp = matDecomp[level];
            int half = waveLength >> 1;

            // Sorting each step in patterns of: { scaling coefficients | wavelet coefficients }
            for (int i = 0; i < half; i++) {
                arrTemp[i] = arrTemp[i + half] = 0.0; // set to zero before sum up
                for (int j = 0; j < 8; j++) {
                    // k = (2*i + j) % waveLength
                    int k = ((i << 1) + j) & (waveLength - 1);
                    // low pass filter for the energy (approximation)
                    arrTemp[i] += arrHilb[k] * scalingDecom[j];
                    // high pass filter for the details
                    arrTemp[i + half] += arrHilb[k] * waveletDecom[j];
                }
            }

            // The rest will be taken from the previous level
            std::copy(arrHilb.begin() + waveLength, arrHilb.begin() + (length - waveLength),
                arrTemp.begin() + waveLength);

            // .. -> 8 -> 4 -> 2 .. shrinks in each step by half wavelength
            waveLength = half;
        }

        return matDecomp;
    }
};
