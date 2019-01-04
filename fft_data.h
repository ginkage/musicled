#ifndef __MUSICLED_FFT_DATA_H__
#define __MUSICLED_FFT_DATA_H__

#include "sliding_window.h"

#include <fftw3.h>

class FftData {
public:
    FftData(int n);
    ~FftData();
    void read(SlidingWindow& window);
    fftw_complex* execute();

private:
    int size;
    fftw_plan plan;
    double* in;
    fftw_complex* out;
};

#endif
