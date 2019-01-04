#include "fft_data.h"

#include <string.h>

FftData::FftData()
    : amp(new double[N1])
{
    out = fftw_alloc_complex(HALF_N);
    in = (double*)out;
    plan = fftw_plan_dft_r2c_1d(N, in, out, FFTW_MEASURE);
    memset(out, 0, HALF_N * sizeof(fftw_complex));
}

FftData::~FftData()
{
    fftw_destroy_plan(plan);
    fftw_free(out);
    delete[] amp;
}

void FftData::read(SlidingWindow& window) { window.read(in, N); }

void FftData::execute() { fftw_execute(plan); }
