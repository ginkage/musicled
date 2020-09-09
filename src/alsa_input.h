#pragma once

#include "circular_buffer.h"
#include "global_state.h"
#include "sample.h"
#include "thread_sync.h"

#include <alsa/asoundlib.h>
#include <memory>
#include <thread>

// ALSA sound input implementation.
// Input sound is written into the respective circular buffers.
// For the one-channel case, left and right buffers will contain the same data.
class AlsaInput {
public:
    AlsaInput(GlobalState* state, std::shared_ptr<ThreadSync> ts);
    ~AlsaInput();

    // Start audio input thread, use the global state to stop it
    void start_thread();

    // Join audio input thread
    void join_thread();

    unsigned int get_rate() { return rate; }
    std::shared_ptr<CircularBuffer<Sample>> get_data() { return samples; }

private:
    void input_alsa();

    int format; // Bits per sample
    unsigned int rate; // Samples per second
    unsigned int channels; // Number of channels

    GlobalState* global; // Global state for thread termination
    std::shared_ptr<CircularBuffer<Sample>> samples; // Audio samples

    snd_pcm_t* handle; // ALSA sound device handle
    snd_pcm_uframes_t frames; // Number of samples per single read
    std::thread thread; // Input thread
    std::shared_ptr<ThreadSync> sync;
};
