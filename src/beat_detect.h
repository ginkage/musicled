#pragma once

#include "circular_buffer.h"
#include "global_state.h"
#include "sample.h"
#include "thread_sync.h"

#include <memory>
#include <thread>

class BeatDetect {
public:
    BeatDetect(
        GlobalState* state, CircularBuffer<Sample>* buf, std::shared_ptr<ThreadSync> ts, int size);

    void start_thread();
    void join_thread();

protected:
    void loop();
    virtual void detect() = 0;

    GlobalState* global; // Global state for thread termination
    CircularBuffer<Sample>* samples; // Audio samples
    std::vector<Sample> values;
    std::thread thread; // Compute thread
    std::shared_ptr<ThreadSync> sync;
};
