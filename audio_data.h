#ifndef __MUSICLED_AUDIO_DATA_H__
#define __MUSICLED_AUDIO_DATA_H__

#include <alsa/asoundlib.h>

#include "sliding_window.h"
#include "color.h"

struct audio_data {
    audio_data();
    ~audio_data();

    int format{ -1 };
    unsigned int rate{ 0 };
    unsigned int channels{ 2 };

    snd_pcm_t* handle;
    snd_pcm_uframes_t frames;

    SlidingWindow<double> left{ 65536 };
    SlidingWindow<double> right{ 65536 };

    bool terminate{ false }; // shared variable used to terminate audio thread
    color curColor;
};

#endif
