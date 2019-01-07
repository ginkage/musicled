#pragma once

#include "circular_buffer.h"

#include <fftw3.h>
#include <vector>

class FftData {
public:
    FftData(int n, CircularBuffer* buf);
    ~FftData();
    void read();
    fftw_complex* execute();

private:
    int size;
    fftw_plan plan;
    std::vector<double> in;
    fftw_complex* out;
    CircularBuffer* buffer;
};
