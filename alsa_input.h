#pragma once

#include "circular_buffer.h"
#include "global_state.h"

#include <alsa/asoundlib.h>
#include <thread>

class AlsaInput {
public:
    AlsaInput(GlobalState* state);
    ~AlsaInput();

    unsigned int get_rate() { return rate; }
    CircularBuffer* get_left() { return &left; }
    CircularBuffer* get_right() { return &right; }

    void start_thread();
    void join_thread();

private:
    void input_alsa();

    int format;
    unsigned int rate;
    unsigned int channels;

    GlobalState* global;
    CircularBuffer left;
    CircularBuffer right;

    snd_pcm_t* handle;
    snd_pcm_uframes_t frames;
    std::thread thread;
};
