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

    /*
     * Performs several 1-D forward transforms from time domain to all possible
     * Hilbert domains using one kind of wavelet transform algorithm for a given
     * array of dimension (length) 2^p | pEN; N = 2, 4, 8, 16, 32, 64, 128, ..,
     * and so on. However, the algorithm stores all levels in a matrix that has in
     * first dimension the range of 0, .., p and in second dimension the
     * coefficients (energy & details) of a certain level. From any level a full
     * reconstruction can be performed. The first dimension is keeping the time
     * series, due to being the Hilbert space of level 0. All following dimensions
     * are keeping the next higher Hilbert spaces, so the next step in wavelet
     * filtering.
     */
    std::vector<std::vector<double>> decompose(std::vector<double>& arrTime)
    {
        int levels = std::ilogb(arrTime.size());
        std::vector<std::vector<double>> matDeComp(levels + 1);
        for (int p = 0; p <= levels; p++)
            matDeComp[p] = forward(arrTime, p);
        return matDeComp;
    }

    /*
     * Performs a 1-D forward transform from time domain to Hilbert domain using
     * one kind of a Fast Wavelet Transform (FWT) algorithm for a given array of
     * dimension (length) 2^p | pEN; N = 2, 4, 8, 16, 32, 64, 128, .., and so on.
     * However, the algorithms stops for a supported level that has be in the
     * range 0, .., p of the dimension of the input array; 0 is the time series
     * itself and p is the maximal number of possible levels.
     */
    std::vector<double> forward(std::vector<double>& arrTime, int level)
    {
        std::vector<double> arrHilb(arrTime);
        int h = arrHilb.size();
        for (int l = 0; h >= 2 && l < level; l++) {
            std::vector<double> arrTempPart = wavelet_forward(arrHilb, h);
            std::copy(arrTempPart.begin(), arrTempPart.begin() + h, arrHilb.begin());
            h = h >> 1;
        }
        return arrHilb;
    }

    /*
     * Performs the forward transform for the given array from time domain to
     * Hilbert domain and returns a new array of the same size keeping
     * coefficients of Hilbert domain and should be of length 2 to the power of p
     * -- length = 2^p where p is a positive integer.
     */
    std::vector<double> wavelet_forward(std::vector<double>& arrTime, int waveLength)
    {
        std::vector<double> arrHilb(waveLength, 0); // set to zero before sum up
        int h = waveLength >> 1; // .. -> 8 -> 4 -> 2 .. shrinks in each step by half wavelength

        for (int i = 0; i < h; i++) {
            // Sorting each step in patterns of: { scaling coefficients | wavelet coefficients }
            for (int j = 0; j < 8; j++) {
                int k = 2 * i + j;
                // circulate over arrays if scaling and wavelet are larger
                while (k >= waveLength)
                    k -= waveLength;

                // low pass filter for the energy (approximation)
                arrHilb[i] += arrTime[k] * scalingDecom[j];
                // high pass filter for the details
                arrHilb[i + h] += arrTime[k] * waveletDecom[j];
            }
        }

        return arrHilb;
    }
};