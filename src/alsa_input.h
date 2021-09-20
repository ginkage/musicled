#pragma once

#include "audio_input.h"

#include <alsa/asoundlib.h>

// ALSA sound input implementation.
class AlsaInput : public AudioInput {
public:
    AlsaInput(GlobalState* state, std::shared_ptr<ThreadSync> ts);
    ~AlsaInput();

private:
    void input_audio() override;

    snd_pcm_t* handle; // ALSA sound device handle
};
