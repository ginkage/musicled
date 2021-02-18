#include "fft_data.h"

#include <cstring>

FftData::FftData(int n, std::shared_ptr<CircularBuffer<Sample>> buf)
    : size(n)
    , buffer(buf)
{
    in = fftwf_alloc_complex(n);
    out = fftwf_alloc_complex(n);
    plan = fftwf_plan_dft_1d(n, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    memset(out, 0, n * sizeof(fftwf_complex));
}

FftData::~FftData()
{
    fftwf_destroy_plan(plan);
    fftwf_free(out);
    fftwf_free(in);
}

Sample* FftData::execute()
{
    buffer->read(reinterpret_cast<Sample*>(in), size);
    fftwf_execute(plan);
    return reinterpret_cast<Sample*>(out);
}
