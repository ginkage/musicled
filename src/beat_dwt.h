#pragma once

#include "beat_detect.h"
#include "wavelet_bpm_detector.h"

class BeatDwt : public BeatDetect {
public:
    BeatDwt(GlobalState* state, CircularBuffer<Sample>* buf, std::shared_ptr<ThreadSync> ts,
        double sampleRate, int windowSize);

protected:
    void detect() override;

    WaveletBPMDetector detector;
};
