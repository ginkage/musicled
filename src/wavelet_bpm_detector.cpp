#include "wavelet_bpm_detector.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <complex>
#include <cstring>
#include <iostream>
#include <numeric>

WaveletBPMDetector::WaveletBPMDetector(double rate, int size)
    : sampleRate(rate)
    , windowSize(size)
    , levels(4)
    , maxPace(1 << (levels - 1))
    , corrSize(size / maxPace)
    , corr(corrSize)
    , decomp(Wavelet::alloc(size, levels))
    , dCMinLength(corrSize / 2)
    , dC(dCMinLength)
    , dCSum(dCMinLength)
{
    in = corr.data();
    out = fftw_alloc_complex(corrSize / 2 + 1);
    plan_forward = fftw_plan_dft_r2c_1d(corrSize, in, out, FFTW_MEASURE);
    plan_back = fftw_plan_dft_c2r_1d(corrSize, out, in, FFTW_MEASURE);
}

WaveletBPMDetector::~WaveletBPMDetector()
{
    fftw_destroy_plan(plan_forward);
    fftw_destroy_plan(plan_back);
    fftw_free(out);
}

/**
 * Identifies the location of data with the maximum absolute
 * value (either positive or negative). If multiple data
 * have the same absolute value the last positive is taken
 * @param data the input array from which to identify the maximum
 * @return the index of the maximum value in the array
 **/
static int detectPeak(std::vector<double>& data, int minIndex, int maxIndex)
{
    double max = DBL_MIN;
    maxIndex = std::min(maxIndex, (int)data.size());

    for (int i = minIndex; i < maxIndex; ++i) {
        max = std::max(max, std::abs(data[i]));
    }

    for (int i = minIndex; i < maxIndex; ++i) {
        if (data[i] == max) {
            return i;
        }
    }

    for (int i = minIndex; i < maxIndex; ++i) {
        if (data[i] == -max) {
            return i;
        }
    }

    return -1;
}

static void undersample(std::vector<double>& data, int pace, std::vector<double>& out)
{
    int length = data.size();
    for (int i = 0, j = 0; j < length; ++i, j += pace) {
        out[i] = data[j];
    }
}

void WaveletBPMDetector::recombine(std::vector<double>& data)
{
    for (double& value : data) {
        value = std::abs(value);
    }

    double mean = std::accumulate(data.begin(), data.end(), 0) / (double)data.size();

    for (int i = 0; i < dCMinLength; ++i) {
        dCSum[i] += data[i] - mean;
    }
}

std::vector<double> WaveletBPMDetector::correlate(std::vector<double>& data)
{
    int n = data.size();
    memcpy(in, data.data(), n * sizeof(double));
    memset(in + n, 0, n * sizeof(double));

    fftw_execute(plan_forward);

    std::complex<double>* cplx = (std::complex<double>*)out;
    for (int i = 0; i <= n; i++) {
        cplx[i] *= std::conj(cplx[i]);
    }

    fftw_execute(plan_back);

    double scale = 1.0 / corrSize;
    for (int i = 0; i < n; i++) {
        data[i] = corr[i] * scale;
    }

    return data;
}

double WaveletBPMDetector::computeWindowBpm(std::vector<double>& data)
{
    int pace = maxPace;
    wavelet.decompose(data, decomp);
    std::fill(dCSum.begin(), dCSum.end(), 0);

    // 4 Level DWT
    for (int loop = 0; loop < levels; ++loop) {
        // Extract envelope from detail coefficients
        //  1) Undersample
        //  2) Absolute value
        //  3) Subtract mean
        undersample(decomp[loop].second, pace, dC);
        recombine(dC);
        pace >>= 1;
    }

    // Add the last approximated data
    recombine(decomp[levels - 1].first);

    // Autocorrelation
    correlate(dCSum);

    // Detect peak in correlated data
    double maxDecimation = maxPace;
    int minIndex = (int)(60.0 / 220.0 * sampleRate / maxDecimation);
    int maxIndex = (int)(60.0 / 40.0 * sampleRate / maxDecimation);
    int location = detectPeak(dCSum, minIndex, maxIndex);

    // Compute window BPM given the peak
    return 60.0 / location * (sampleRate / maxDecimation);
}
