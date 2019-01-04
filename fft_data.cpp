#include "fft_data.h"

#include <string.h>

FftData::FftData(int n)
    : size(n)
{
    int half_n = n / 2 + 1;
    out = fftw_alloc_complex(half_n);
    in = (double*)out;
    plan = fftw_plan_dft_r2c_1d(n, in, out, FFTW_MEASURE);
    memset(out, 0, half_n * sizeof(fftw_complex));
}

FftData::~FftData()
{
    fftw_destroy_plan(plan);
    fftw_free(out);
}

void FftData::read(SlidingWindow& window) { window.read(in, size); }

fftw_complex* FftData::execute()
{
    fftw_execute(plan);
    return out;
}
