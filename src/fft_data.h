#pragma once

#include "circular_buffer.h"
#include "sample.h"

#include <fftw3.h>
#include <vector>

// FFTW3 computational engine for the specified circular buffer.
class FftData {
public:
    FftData(int n, CircularBuffer<Sample>* buf);
    ~FftData();

    // Read the latest N samples from the circular buffer
    void read();

    // Compute the FFT output, returns an array
    fftw_complex* execute();

private:
    int size; // Number of samples to analyze
    fftw_plan plan; // FFT calculation parameters
    fftw_complex* in; // Array to read input data into
    fftw_complex* out; // Memory-aligned array to store calculation result
    CircularBuffer<Sample>* buffer; // Buffer to read samples from
};
