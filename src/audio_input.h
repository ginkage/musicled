#pragma once

#include "circular_buffer.h"
#include "global_state.h"
#include "sample.h"
#include "thread_sync.h"

#include <memory>
#include <thread>

// Sound input interface.
// Input sound is written into a circular buffer.
class AudioInput {
public:
    AudioInput(GlobalState* state, std::shared_ptr<ThreadSync> ts);
    ~AudioInput() = default;

    // Start audio input thread, use the global state to stop it
    void start_thread();

    // Join audio input thread
    void join_thread();

    unsigned int get_rate() { return rate; }
    std::shared_ptr<CircularBuffer<Sample>> get_data() { return samples; }

protected:
    virtual void input_audio() = 0;

    GlobalState* global; // Global state for thread termination
    std::shared_ptr<ThreadSync> sync;

    std::thread thread; // Input thread

    int format { 16 }; // Bits per sample
    unsigned int rate { 44100 }; // Samples per second
    unsigned long frames { 256 }; // Number of samples per single read
    unsigned int channels { 2 }; // Number of channels

    std::shared_ptr<CircularBuffer<Sample>> samples; // Audio samples
};
