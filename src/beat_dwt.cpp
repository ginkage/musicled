#include "beat_dwt.h"

#include <iostream>
#include <numeric>

BeatDwt::BeatDwt(GlobalState* state, CircularBuffer<Sample>* buf, std::shared_ptr<ThreadSync> ts,
    double sampleRate, int windowSize)
    : BeatDetect(state, buf, ts, windowSize)
    , detector(sampleRate, windowSize)
{
}

void BeatDwt::detect()
{
    // Latest values, single channel, denormalized
    const double norm = std::sqrt(2.0);
    std::vector<double> data(values.size());
    for (unsigned int i = 0; i < values.size(); ++i) {
        data[i] = values[i].real() * norm;
    }

    double bpm = detector.computeWindowBpm(data);
    std::cout << "Window BPM: " << bpm << std::endl << std::flush;
}
