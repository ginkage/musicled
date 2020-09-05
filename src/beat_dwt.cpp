#include "beat_dwt.h"

#include <iostream>
#include <numeric>

BeatDwt::BeatDwt(GlobalState* state, CircularBuffer<Sample>* buf, std::shared_ptr<ThreadSync> ts,
    double sampleRate)
    : BeatDetect(state, buf, ts, 131072)
    , detector(sampleRate)
{
}

void BeatDwt::detect()
{
    // Latest values, single channel
    std::vector<double> data(values.size());
    for (unsigned int i = 0; i < values.size(); ++i) {
        data[i] = values[i].real();
    }

    double bpm = detector.computeWindowBpm(data);
    std::cout << "Window BPM: " << bpm << std::endl << std::flush;
}
