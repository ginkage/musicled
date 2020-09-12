#pragma once

#include "beat_detect.h"
#include "freq_data.h"
#include "wavelet_bpm_detector.h"

#include <memory>

class BeatDwt : public BeatDetect {
public:
    BeatDwt(GlobalState* state, std::shared_ptr<CircularBuffer<Sample>> buf,
        std::shared_ptr<ThreadSync> ts, std::shared_ptr<FreqData> freq, double sampleRate,
        int windowSize);

protected:
    void detect() override;

    WaveletBPMDetector detector;
};
