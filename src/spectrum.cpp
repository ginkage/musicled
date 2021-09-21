#include "spectrum.h"

#include "pulse_input.h"

using steady_clock = std::chrono::steady_clock;

Spectrum::Spectrum(GlobalState* state, int N)
    : global(state)
    , sync(new ThreadSync())
    , audio(new PulseInput(state, sync))
    , freq(new FreqData(N, audio->get_rate()))
    , fft(N, audio->get_data())
    , beat(state, audio->get_data(), sync, freq, audio->get_rate(), 131072)
    , slide(std::chrono::milliseconds(250))
{
    audio->start_thread();
    beat.start_thread();
}

Spectrum::~Spectrum()
{
    beat.join_thread();
    audio->join_thread();
}

FreqData& Spectrum::process()
{
    // Compute FFT for both channels
    Sample* out = fft.execute();

    int maxF = freq->minK;
    float maxAmp = 0;

    // Find the loudest frequency
    for (int k = freq->minK; k < freq->maxK; k++) {
        float amp = freq->amp[k] = std::abs(out[k]);
        if (amp > maxAmp) {
            maxAmp = amp;
            maxF = k;
        }
    }

    // Adjust window size according to the current detected BPM
    steady_clock::duration half = std::chrono::seconds(30);
    slide.set_window_size(std::chrono::duration_cast<steady_clock::duration>(half / global->bpm));

    // Set the global color accordingly
    global->cur_color.ic
        = slide.offer(std::make_pair(freq->color[maxF].ic, std::chrono::steady_clock::now()));
    return *freq;
}
