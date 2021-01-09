#include "beat_dwt.h"

//#include <iostream>
#include <numeric>

BeatDwt::BeatDwt(GlobalState* state, std::shared_ptr<CircularBuffer<Sample>> buf,
    std::shared_ptr<ThreadSync> ts, std::shared_ptr<FreqData> freq, double sampleRate,
    int windowSize)
    : BeatDetect(state, buf, ts, windowSize)
    , detector(sampleRate, windowSize, freq)
    , slide(std::chrono::seconds(5))
{
}

void BeatDwt::detect()
{
    // Latest values, absolute
    std::vector<double> data(values.size());
    for (unsigned int i = 0; i < values.size(); ++i) {
        data[i] = std::abs(values[i]);
    }

    double bpm = detector.computeWindowBpm(data);
    bpm = slide.offer(std::make_pair(bpm, std::chrono::steady_clock::now()));
    if (!global->lock_bpm) {
        global->bpm = bpm;
    }
    // std::cout << "Window BPM: " << bpm << std::endl << std::flush;
}
