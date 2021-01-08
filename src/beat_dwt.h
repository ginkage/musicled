#pragma once

#include "beat_detect.h"
#include "freq_data.h"
#include "sliding_median.h"
#include "wavelet_bpm_detector.h"

#include <chrono>
#include <memory>

class BeatDwt : public BeatDetect {
public:
    BeatDwt(GlobalState* state, std::shared_ptr<CircularBuffer<Sample>> buf,
        std::shared_ptr<ThreadSync> ts, std::shared_ptr<FreqData> freq, double sampleRate,
        int windowSize);

protected:
    void detect() override;

    WaveletBPMDetector detector;

    using Timestamp = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::steady_clock::duration;
    SlidingMedian<double, Timestamp, Duration> slide;
};
