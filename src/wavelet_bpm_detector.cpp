#include "wavelet_bpm_detector.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <complex>
#include <cstring>
#include <iostream>
#include <numeric>

WaveletBPMDetector::WaveletBPMDetector(double rate, int size)
    : levels(4)
    , corrSize(size / (1 << (levels - 1)))
    , corr(corrSize)
    , sampleRate(rate)
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

static std::vector<double> undersample(std::vector<double>& data, int pace)
{
    int length = data.size();
    std::vector<double> result(length / pace);
    for (int i = 0, j = 0; j < length; ++i, j += pace) {
        result[i] = data[j];
    }
    return result;
}

static std::vector<double> abs(std::vector<double>& data)
{
    for (double& value : data) {
        value = std::abs(value);
    }
    return data;
}

static std::vector<double> normalize(std::vector<double>& data)
{
    double mean = std::accumulate(data.begin(), data.end(), 0) / (double)data.size();
    for (double& value : data) {
        value -= mean;
    }
    return data;
}

static void add(std::vector<double>& data, std::vector<double>& plus)
{
    for (unsigned int i = 0; i < data.size(); ++i) {
        data[i] += plus[i];
    }
}

std::vector<double> WaveletBPMDetector::correlate_fft(std::vector<double>& data)
{
    int n = corrSize / 2;
    memset(in, 0, corrSize * sizeof(double));
    memcpy(in, data.data(), n * sizeof(double));

    fftw_execute(plan_forward);

    std::complex<double>* cplx = (std::complex<double>*)out;
    for (int i = 0; i <= n; i++) {
        cplx[i] *= std::conj(cplx[i]);
    }

    fftw_execute(plan_back);

    for (int i = 0; i < n; i++) {
        corr[i] /= corrSize;
    }

    return std::vector<double>(in, in + n);
}

std::vector<double> WaveletBPMDetector::correlate_brute(std::vector<double>& data)
{
    int n = data.size();
    std::vector<double> correlation(n, 0);
    for (int k = 0; k < n; ++k) {
        for (int i = 0; k + i < n; ++i) {
            correlation[k] += data[i] * data[k + i];
        }
    }
    return correlation;
}

double WaveletBPMDetector::computeWindowBpm(std::vector<double>& data)
{
    int pace = 1 << (levels - 1);
    double maxDecimation = pace;
    int minIndex = (int)(60.0 / 220.0 * sampleRate / maxDecimation);
    int maxIndex = (int)(60.0 / 40.0 * sampleRate / maxDecimation);

    std::vector<decomposition> decomp = wavelet.decompose(data, levels);
    int dCMinLength = int(decomp[0].second.size() / maxDecimation);
    std::vector<double> dCSum(dCMinLength, 0);
    std::vector<double> dC;

    // 4 Level DWT
    for (int loop = 0; loop < levels; ++loop) {
        // Extract envelope from detail coefficients
        //  1) Undersample
        //  2) Absolute value
        //  3) Subtract mean
        dC = undersample(decomp[loop].second, pace);
        dC = abs(dC);
        dC = normalize(dC);

        // Recombine detail coeffients
        add(dCSum, dC);

        pace >>= 1;
    }

    // Add the last approximated data
    std::vector<double> aC = abs(decomp[levels - 1].first);
    aC = normalize(aC);
    add(dCSum, aC);

    // Autocorrelation
    std::vector<double> correlated = correlate_fft(dCSum);

    // Detect peak in correlated data
    int location = detectPeak(correlated, minIndex, maxIndex);

    // Compute window BPM given the peak
    return 60.0 / location * (sampleRate / maxDecimation);
}
