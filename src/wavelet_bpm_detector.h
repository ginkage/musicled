#pragma once

#include "freq_data.h"
#include "wavelet.h"

#include <fftw3.h>
#include <memory>
#include <vector>

/**
 * Class <code>WaveletBPMDetector</code> can be used to
 * detect the tempo of a track in beats-per-minute.
 * The class implements the algorithm presented by
 * Tzanetakis, Essl and Cookin the paper titled
 * "Audio Analysis using the Discrete Wavelet Transform"
 *
 * To detect the tempo the discrete wavelet transform is used.
 * Track samples are divided into windows of frames.
 * For each window data are divided into 4 frequency sub-bands
 * through DWT. For each frequency sub-band an envelope is
 * extracted from the detail coffecients by:
 * 1) Full wave rectification (take the absolute value),
 * 2) Downsampling of the coefficients,
 * 3) Normalization (via mean removal)
 * These 4 sub-band envelopes are then summed together.
 * The resulting collection of data is then autocorrelated.
 * Peaks in the correlated data correspond to peaks in the
 * original signal.
 * then peaks are identified on the filtered data.
 * Given the position of such a peak the approximated
 * tempo of the window is computed and appended to a colletion.
 * Once all windows in the track are processed the beat-per-minute
 * value is returned as the median of the windows values.
 **/
class WaveletBPMDetector {
public:
    WaveletBPMDetector(int rate, int size, std::shared_ptr<FreqData> data);
    ~WaveletBPMDetector();

    /**
     * Given <code>windowFrames</code> samples computes a BPM
     * value for the window and pushes it in <code>instantBpm</code>
     * @param An array of <code>windowFrames</code> samples representing the window
     **/
    float computeWindowBpm(std::vector<float>& data);

    // For testing
    std::vector<float> correlate(std::vector<float>& data);

private:
    void recombine(std::vector<float>& data);
    int detectPeak(std::vector<float>& data);

    int sampleRate;
    int windowSize;

    int levels;
    int maxPace;
    int corrSize;
    std::vector<float> corr;
    Wavelet wavelet;
    int dCMinLength;
    std::vector<float> dC;
    std::vector<float> dCSum;
    std::vector<float> straight;
    float minute;
    int minIndex;
    int maxIndex;

    // Autocorrelation
    float* in;
    fftwf_complex* out;
    fftwf_plan plan_forward;
    fftwf_plan plan_back;

    std::shared_ptr<FreqData> freq;
};
