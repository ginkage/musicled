#include "fft_data.h"

#include <string.h>

FftData::FftData(int n, CircularBuffer* buf)
    : size(n)
    , in(n)
    , buffer(buf)
{
    int half_n = n / 2 + 1;
    out = fftw_alloc_complex(half_n);
    plan = fftw_plan_dft_r2c_1d(n, in.data(), out, FFTW_MEASURE);
    memset(out, 0, half_n * sizeof(fftw_complex));
}

FftData::~FftData()
{
    fftw_destroy_plan(plan);
    fftw_free(out);
}

void FftData::read() { buffer->read(in, size); }

fftw_complex* FftData::execute()
{
    fftw_execute(plan);
    return out;
}
