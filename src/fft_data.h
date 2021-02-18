#pragma once

#include "circular_buffer.h"
#include "sample.h"

#include <fftw3.h>
#include <memory>
#include <vector>

// FFTW3 computational engine for the specified circular buffer.
class FftData {
public:
    FftData(int n, std::shared_ptr<CircularBuffer<Sample>> buf);
    ~FftData();

    // Compute the FFT output using the latest N samples from the circular buffer
    Sample* execute();

private:
    int size; // Number of samples to analyze
    fftwf_plan plan; // FFT calculation parameters
    fftwf_complex* in; // Array to read input data into
    fftwf_complex* out; // Memory-aligned array to store calculation result
    std::shared_ptr<CircularBuffer<Sample>> buffer; // Buffer to read samples from
};
