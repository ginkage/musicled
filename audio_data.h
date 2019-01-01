#ifndef __MUSICLED_AUDIO_DATA_H__
#define __MUSICLED_AUDIO_DATA_H__

#include "sliding_window.h"

#include <alsa/asoundlib.h>
#include <pthread.h>

struct audio_data {
public:
    audio_data();
    ~audio_data();

    unsigned int rate{ 0 };
    unsigned int channels{ 2 };

    SlidingWindow<double> left{ 65536 };
    SlidingWindow<double> right{ 65536 };

    void start_thread();
    void join_thread();

private:
    static void* run_thread(void* arg);
    void input_alsa();

    int format{ -1 };
    snd_pcm_t* handle;
    snd_pcm_uframes_t frames;
    pthread_t p_thread;
};

#endif
