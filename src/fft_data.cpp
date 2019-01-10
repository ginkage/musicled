#include "fft_data.h"

#include <string.h>

FftData::FftData(int n, CircularBuffer* buf)
    : size(n)
    , buffer(buf)
{
    int half_n = n / 2 + 1;
    in = (double*)fftw_malloc(n * sizeof(double));
    out = fftw_alloc_complex(half_n);
    plan = fftw_plan_dft_r2c_1d(n, in, out, FFTW_MEASURE);
    memset(out, 0, half_n * sizeof(fftw_complex));
}

FftData::~FftData()
{
    fftw_destroy_plan(plan);
    fftw_free(out);
    fftw_free(in);
}

void FftData::read() { buffer->read(in, size); }

fftw_complex* FftData::execute()
{
    fftw_execute(plan);
    return out;
}
