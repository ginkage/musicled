#ifndef __MUSICLED_AUDIO_DATA_H__
#define __MUSICLED_AUDIO_DATA_H__

#include "global_state.h"
#include "sliding_window.h"

#include <alsa/asoundlib.h>
#include <pthread.h>

class AlsaInput {
public:
    AlsaInput(GlobalState* state);
    ~AlsaInput();

    unsigned int rate{ 0 };

    SlidingWindow left{ 65536 };
    SlidingWindow right{ 65536 };

    void start_thread();
    void join_thread();

private:
    static void* run_thread(void* arg);
    void input_alsa();

    unsigned int channels{ 2 };
    int format{ -1 };
    snd_pcm_t* handle;
    snd_pcm_uframes_t frames;
    GlobalState* global;
    pthread_t p_thread;
};

#endif
