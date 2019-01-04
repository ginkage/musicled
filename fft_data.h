#ifndef __MUSICLED_FFT_DATA_H__
#define __MUSICLED_FFT_DATA_H__

#include "sliding_window.h"

#include <fftw3.h>

constexpr int M = 11;
constexpr int N = 1 << M;
constexpr int N1 = N / 2;
constexpr int HALF_N = N / 2 + 1;

class FftData {
public:
    FftData();
    ~FftData();
    void read(SlidingWindow& window);
    void execute();

public:
    fftw_complex* out;
    double* amp;

private:
    fftw_plan plan;
    double* in;
};

#endif
