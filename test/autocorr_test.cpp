#include "../src/wavelet_bpm_detector.h"

#include <chrono>
#include <iostream>
#include <numeric>
#include <random>

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

static std::vector<double> correlate_brute(std::vector<double>& data)
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

int main()
{
    std::default_random_engine generator;
    std::uniform_real_distribution<double> distribution(-32768.0, 32767.0);

    std::vector<double> data(16384);
    for (unsigned int i = 0; i < data.size(); ++i) {
        data[i] = distribution(generator);
    }

    data = abs(data);
    data = normalize(data);

    WaveletBPMDetector detector(44100, 262144, std::make_shared<FreqData>(FreqData(1, 1)));
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    std::vector<double> corr_brute = correlate_brute(data);

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Time (brute) = "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() << "[ns]"
              << std::endl;

    begin = std::chrono::steady_clock::now();

    std::vector<double> corr_fft = detector.correlate(data);

    end = std::chrono::steady_clock::now();
    std::cout << "Time (fftw) = "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() << "[ns]"
              << std::endl;

    double sum_error = 0;
    for (unsigned int i = 0; i < corr_brute.size(); ++i) {
        sum_error += std::abs((corr_brute[i] - corr_fft[i]) / corr_brute[i]);
    }
    double avg_error = sum_error / corr_brute.size();

    std::cout << "Rel. error: " << avg_error << std::endl;
}
