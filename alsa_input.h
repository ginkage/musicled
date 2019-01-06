#ifndef __MUSICLED_ALSA_INPUT_H__
#define __MUSICLED_ALSA_INPUT_H__

#include "global_state.h"
#include "sliding_window.h"

#include <alsa/asoundlib.h>
#include <thread>

class AlsaInput {
public:
    AlsaInput(GlobalState* state);
    ~AlsaInput();

    unsigned int get_rate() { return rate; }
    SlidingWindow* get_left() { return &left; }
    SlidingWindow* get_right() { return &right; }

    void start_thread();
    void join_thread();

private:
    void input_alsa();

    int format;
    unsigned int rate;
    unsigned int channels;

    GlobalState* global;
    SlidingWindow left;
    SlidingWindow right;

    snd_pcm_t* handle;
    snd_pcm_uframes_t frames;
    std::thread thread;
};

#endif
