#pragma once

#include "sliding_window.h"

#include <fftw3.h>
#include <vector>

class FftData {
public:
    FftData(int n, SlidingWindow* win);
    ~FftData();
    void read();
    fftw_complex* execute();

private:
    int size;
    fftw_plan plan;
    std::vector<double> in;
    fftw_complex* out;
    SlidingWindow* window;
};
