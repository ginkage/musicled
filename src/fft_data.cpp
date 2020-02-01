#include "fft_data.h"

#include <string.h>

FftData::FftData(int n, CircularBuffer<std::complex<double>>* buf)
    : size(n)
    , buffer(buf)
{
    in = fftw_alloc_complex(n);
    out = fftw_alloc_complex(n);
    plan = fftw_plan_dft_1d(n, in, out, FFTW_FORWARD, FFTW_MEASURE);
    memset(out, 0, n * sizeof(fftw_complex));
}

FftData::~FftData()
{
    fftw_destroy_plan(plan);
    fftw_free(out);
    fftw_free(in);
}

void FftData::read() { buffer->read(reinterpret_cast<std::complex<double>*>(in), size); }

fftw_complex* FftData::execute()
{
    fftw_execute(plan);
    return out;
}
