#include "wavelet_bpm_detector.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <complex>
#include <cstring>
#include <numeric>

WaveletBPMDetector::WaveletBPMDetector(int rate, int size, std::shared_ptr<FreqData> data)
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
    , minute(sampleRate * 60.0 / maxPace)
    , minIndex(minute / 220.0)
    , maxIndex(minute / 40.0)
    , in(corr.data())
    , out(fftw_alloc_complex(corrSize / 2 + 1))
    , plan_forward(fftw_plan_dft_r2c_1d(corrSize, in, out, FFTW_MEASURE))
    , plan_back(fftw_plan_dft_c2r_1d(corrSize, out, in, FFTW_MEASURE))
    , freq(data)
{
    freq->wx = std::vector<double>(maxIndex - minIndex);
    freq->wy = std::vector<double>(maxIndex - minIndex);
    double nom = 1.0 / (1.0 / minIndex - 1.0 / maxIndex);
    double start = nom / maxIndex;
    for (int i = minIndex; i < maxIndex; ++i) {
        freq->wx[i - minIndex] = nom / i - start;
    }
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
int WaveletBPMDetector::detectPeak(std::vector<double>& data)
{
    double max = DBL_MIN;
    maxIndex = std::min(maxIndex, (int)data.size());

    // Straighten the curve
    double start = data[minIndex] / (maxIndex - minIndex);
    for (int i = minIndex; i < maxIndex; ++i) {
        data[i] = data[i] - (maxIndex - i) * start;
    }

    for (int i = minIndex; i < maxIndex; ++i) {
        max = std::max(max, std::fabs(data[i]));
    }

    double scale = 1.0 / max;
    for (int i = minIndex; i < maxIndex; ++i) {
        freq->wy[i - minIndex] = data[i] * scale;
    }
    freq->ready = true;

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
        value = std::fabs(value);
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
    wavelet.decompose(data, decomp);
    std::fill(dCSum.begin(), dCSum.end(), 0);

    // 4 Level DWT
    for (int loop = 0, pace = maxPace; loop < levels; ++loop, pace >>= 1) {
        // Extract envelope from detail coefficients
        //  1) Undersample
        //  2) Absolute value
        //  3) Subtract mean
        undersample(decomp[loop].second, pace, dC);
        recombine(dC);
    }

    // Add the last approximated data
    recombine(decomp[levels - 1].first);

    // Autocorrelation
    correlate(dCSum);

    // Detect peak in correlated data
    int location = detectPeak(dCSum);

    // Compute window BPM given the peak
    return minute / location;
}
