#ifndef __MUSICLED_FFT_DATA_H__
#define __MUSICLED_FFT_DATA_H__

#include "sliding_window.h"

#include <fftw3.h>

class FftData {
public:
    FftData(int n, SlidingWindow* win);
    ~FftData();
    void read();
    fftw_complex* execute();

private:
    int size;
    fftw_plan plan;
    double* in;
    fftw_complex* out;
    SlidingWindow* window;
};

#endif
