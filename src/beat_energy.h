#pragma once

#include "beat_detect.h"
#include "circular_buffer.h"

class BeatEnergy : public BeatDetect {
public:
    BeatEnergy(GlobalState* state, CircularBuffer<Sample>* buf, std::shared_ptr<ThreadSync> ts);

protected:
    void detect() override;

    CircularBuffer<double> energy;
};
