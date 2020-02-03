#pragma once

#include "circular_buffer.h"
#include "global_state.h"
#include "sample.h"
#include "thread_sync.h"

#include <memory>
#include <thread>

class BeatEnergy {
public:
    BeatEnergy(GlobalState* state, CircularBuffer<Sample>* buf, std::shared_ptr<ThreadSync> ts);

    void start_thread();
    void join_thread();

private:
    void beat_detect();

    GlobalState* global; // Global state for thread termination
    CircularBuffer<Sample>* samples; // Audio samples
    std::vector<Sample> values;
    std::thread thread; // Compute thread
    std::shared_ptr<ThreadSync> sync;
};
